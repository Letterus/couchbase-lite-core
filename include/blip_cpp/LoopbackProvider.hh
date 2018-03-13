//
// LoopbackProvider.hh
//
// Copyright (c) 2017 Couchbase, Inc All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#pragma once
#include "MockProvider.hh"
#include <atomic>
#include <chrono>


namespace litecore { namespace websocket {
    class LoopbackProvider;

    static constexpr size_t kSendBufferSize = 32 * 1024;


    /** A WebSocket connection that relays messages to another instance of LoopbackWebSocket. */
    class LoopbackWebSocket : public MockWebSocket {
    protected:
        class Driver;
        actor::delay_t _latency;

    public:
        LoopbackWebSocket(Provider &provider, const Address &address, actor::delay_t latency)
        :MockWebSocket(provider, address)
        ,_latency(latency)
        { }

        virtual MockWebSocket::Driver* createDriver() override {
            return new Driver(this, _latency);
        }

        LoopbackWebSocket::Driver* driver() const {
            return (Driver*)_driver.get();
        }

        void bind(LoopbackWebSocket *peer, const fleeceapi::AllocedDict &responseHeaders) {
            Assert(!_driver);
            _driver = createDriver();
            driver()->bind(peer, responseHeaders);
        }

        virtual void connect() override {
            Assert(driver()->_peer);
            MockWebSocket::connect();
        }

        virtual bool send(fleece::slice msg, bool binary) override {
            auto newValue = (driver()->_bufferedBytes += msg.size);
            MockWebSocket::send(msg, binary);
            return newValue <= kSendBufferSize;
        }

        virtual void ack(size_t msgSize) {
            driver()->enqueue(&Driver::_ack, msgSize);
        }

    protected:

        class Driver : public MockWebSocket::Driver {
        public:

            Driver(LoopbackWebSocket *ws, actor::delay_t latency)
            :MockWebSocket::Driver(ws)
            ,_latency(latency)
            { }

            virtual std::string loggingClassName() const override {
                return "LoopbackWS";
            }

            void bind(LoopbackWebSocket *peer, const fleeceapi::AllocedDict &responseHeaders) {
                // Called by LoopbackProvider::bind, which is called before my connect() method,
                // so it's safe to set the member variables directly instead of on the actor queue.
                _peer = peer;
                _responseHeaders = responseHeaders;
            }

            virtual void _connect() override {
                _simulateHTTPResponse(200, _responseHeaders);
                _webSocket->simulateConnected(_latency);
            }

            virtual void _send(fleece::alloc_slice msg, bool binary) override {
                if (_peer) {
                    logDebug("SEND: %s", formatMsg(msg, binary).c_str());
                    _peer->simulateReceived(msg, binary, _latency);
                } else {
                    log("SEND: Failed, socket is closed");
                }
            }

            virtual void _simulateReceived(fleece::alloc_slice msg, bool binary) override {
                if (!connected())
                    return;
                MockWebSocket::Driver::_simulateReceived(msg, binary);
                _peer->ack(msg.size);
            }

            virtual void _ack(size_t msgSize) {
                if (!connected())
                    return;
                auto newValue = (_bufferedBytes -= msgSize);
                if (newValue <= kSendBufferSize && newValue + msgSize > kSendBufferSize) {
                    logVerbose("WRITEABLE");
                    _webSocket->delegate().onWebSocketWriteable();
                }
            }

            virtual void _close(int status, fleece::alloc_slice message) override {
                std::string messageStr(message);
                log("CLOSE; status=%d", status);
                if (_peer)
                    _peer->simulateClosed(kWebSocketClose, status, messageStr.c_str(), _latency);
                MockWebSocket::Driver::_close(status, message);
            }

            virtual void _closed() override {
                log("_closed()");
                _peer = nullptr;
                MockWebSocket::Driver::_closed();
            }

        private:
            friend class LoopbackWebSocket;

            actor::delay_t _latency {0.0};
            Retained<LoopbackWebSocket> _peer;
            fleeceapi::AllocedDict _responseHeaders;
            std::atomic<size_t> _bufferedBytes {0};
        };
    };


    /** A WebSocketProvider that creates pairs of WebSocket objects that talk to each other. */
    class LoopbackProvider : public MockProvider {
    public:

        /** Constructs a WebSocketProvider. A latency time can be provided, which is the delay
            before a message sent by one connection is received by its peer. */
        LoopbackProvider(actor::delay_t latency = actor::delay_t::zero())
        :_latency(latency)
        { }

        LoopbackWebSocket* createWebSocket(const Address &address,
                                           const fleeceapi::AllocedDict &options ={}) override {
            return new LoopbackWebSocket(*this, address, _latency);
        }

        /** Binds two LoopbackWebSocket objects to each other, so after they open, each will 
            receive messages sent by the other. When one closes, the other will receive a close
            event.
            MUST be called before the socket objects' connect() methods are called! */
        void bind(WebSocket *c1, WebSocket *c2,
                  const fleeceapi::AllocedDict &responseHeaders ={})
        {
            auto lc1 = dynamic_cast<LoopbackWebSocket*>(c1);
            auto lc2 = dynamic_cast<LoopbackWebSocket*>(c2);
            lc1->bind(lc2, responseHeaders);
            lc2->bind(lc1, responseHeaders);
        }

    private:
        actor::delay_t _latency {0.0};
    };


} }
