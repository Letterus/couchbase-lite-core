//
// TCPSocket.hh
//
// Copyright 2019-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#pragma once
#include "RefCounted.hh"
#include "Address.hh"
#include "HTTPTypes.hh"
#include "fleece/Fleece.hh"
#include <functional>
#include <memory>
#include <vector>

namespace litecore::crypto {
    class Cert;
}
namespace litecore::websocket {
    class Headers;
}
namespace sockpp {
    class stream_socket;
}

namespace litecore::net {
    class TLSContext;
    class HTTPLogic;

    /** TCP socket class, using the sockpp library. */
    class TCPSocket {
    public:
        using slice = fleece::slice;

        explicit TCPSocket(bool isClient, TLSContext* =nullptr);
        virtual ~TCPSocket();

        /// Initializes TCPSocket, must call at least once before using any
        /// socket related functionality
        static void initialize();

        /// Returns the TLS context, if any, used by this socket.
        TLSContext* tlsContext();

        /// Closes the socket if it's open.
        void close();

        bool connected() const;
        operator bool() const                   {return connected();}

        void onClose(std::function<void()> &&callback)     {_onClose = move(callback);}

        /// Peer's address: IP address + ":" + port number
        std::string peerAddress();

        /// Raw X.509 data of peer's TLS certificate (if it has one)
        std::string peerTLSCertificateData();

        /// Peer's TLS certificate (if it has one)
        fleece::Retained<crypto::Cert> peerTLSCertificate();

        /// Last error
        C4Error error() const                   {return _error;}

        //-------- READING:

        /// Reads up to \ref byteCount bytes to the location \ref dst.
        ///
        /// - In blocking mode: Returning zero means that EOF was reached.
        /// - In non-blocking mode: Returning zero means either that EOF was reached or
        ///   EWOULDBLOCK error was returned. Use atReadEOF() to confirm whether the EOF was
        ///   reached or not.
        ///
        /// On other error returns -1.
        ssize_t read(void *dst, size_t byteCount) MUST_USE_RESULT;

        /// Reads exactly \ref byteCount bytes to the location \ref dst.
        /// On premature EOF returns 0 and sets error {WebSocket, 400}.
        ssize_t readExactly(void *dst, size_t byteCount) MUST_USE_RESULT;

        static constexpr size_t kMaxDelimitedReadSize = 50 * 1024;

        /// Reads from the socket until the \ref delimiter byte sequence is found,
        /// and returns the bytes read.
        /// If the delimiter is not found, due to EOF of reading more than \ref maxSize bytes,
        /// sets an error and returns nullslice.
        /// @param delimiter  The byte sequence that ends the data.
        /// @param includeDelimiter  If true, delimiter is included at end of returned data.
        /// @param maxSize  Maximum number of bytes to read; if exceeded, sets error & returns null.
        fleece::alloc_slice readToDelimiter(slice delimiter,
                                            bool includeDelimiter =true,
                                            size_t maxSize =kMaxDelimitedReadSize) MUST_USE_RESULT;

        /// Reads all remaining data until EOF on the read side.
        fleece::alloc_slice readToEOF();

        /// Reads HTTP "Transfer-Encoding: chunked" response data.
        fleece::alloc_slice readChunkedHTTPBody();

        /// Reads an HTTP body, given the headers.
        /// Supports explicit Content-Length, Chunked transfer encoding, or old-school read-to-EOF.
        /// On error, sets the error property and returns false.
        bool readHTTPBody(const websocket::Headers &headers, fleece::alloc_slice &body) MUST_USE_RESULT;

        bool atReadEOF() const                          {return _eofOnRead;}

        //-------- WRITING:

        /// Writes to the socket and returns the number of bytes written:
        ssize_t write(slice) MUST_USE_RESULT;

        /// Writes all the bytes to the socket.
        ssize_t write_n(slice) MUST_USE_RESULT;

        /// Writes multiple byte ranges (slices) to the socket.
        /// Those that are completely written are removed from the head of the vector.
        /// One that's partially written has its `buf` and `size` adjusted to cover only the
        /// unsent bytes. (This will always be the 1st in the vector on return.)
        ssize_t write(std::vector<fleece::slice> &ioByteRanges) MUST_USE_RESULT;

        bool atWriteEOF() const                         {return _eofOnWrite;}

        //-------- [NON]BLOCKING AND WAITING:

        /// Sets read/write/connect timeout in seconds
        bool setTimeout(double secs);
        double timeout() const                  {return _timeout;}

        /// Enables or disables non-blocking mode.
        bool setNonBlocking(bool);

        void onReadable(std::function<void()>);
        void onWriteable(std::function<void()>);
        void onDisconnect(std::function<void()>);
        void cancelCallbacks();

    protected:
        bool setSocket(std::unique_ptr<sockpp::stream_socket>);
        void setError(C4ErrorDomain, int code, slice message =fleece::nullslice);
        bool wrapTLS(slice hostname);
        void checkStreamError();
        bool checkSocketFailure();
        ssize_t _read(void *dst, size_t byteCount) MUST_USE_RESULT;
        void pushUnread(slice);
        int fileDescriptor();

    private:
        bool _setTimeout(double secs);
        sockpp::stream_socket* actualSocket() const;
        void addListener(int pollerEvent /*Poller::Event*/, std::function<void()>&&);

        std::unique_ptr<sockpp::stream_socket> _socket;     // The TCP (or TLS) socket
        fleece::Retained<TLSContext> _tlsContext;           // Custom TLS context if any
        bool const _isClient;                               // Am I a client (not server) socket?
        bool _nonBlocking {false};                          // Is socket in non-blocking mode?
        double _timeout {0};                                // read/write/connect timeout in seconds
        C4Error _error {};                                  // last error
        fleece::alloc_slice _unread;                        // Data read that's been "pushed back"
        size_t _unreadLen {0};                              // Length of valid data in _unread
        bool _eofOnRead {false};                            // Has read stream reached EOF?
        bool _eofOnWrite {false};                           // Has write stream reached EOF?
        std::function<void()> _onClose;
    };



    /** A client socket, that opens a TCP connection. */
    class ClientSocket : public TCPSocket {
    public:
        explicit ClientSocket(TLSContext* =nullptr);

        /// Connects to the host, synchronously. On failure throws an exception.
        bool connect(const Address &addr) MUST_USE_RESULT;

        /// Wrap the existing socket in TLS, performing a handshake.
        /// This is used after connecting to a CONNECT-type proxy, not in a normal connection.
        bool wrapTLS(slice hostname)        {return TCPSocket::wrapTLS(hostname);}
    };

    

    /** A server-side socket, that handles a client connection. */
    class ResponderSocket : public TCPSocket {
    public:
        explicit ResponderSocket(TLSContext* =nullptr);

        bool acceptSocket(sockpp::stream_socket&&) MUST_USE_RESULT;
        bool acceptSocket(std::unique_ptr<sockpp::stream_socket>) MUST_USE_RESULT;

        /// Perform server-side TLS handshake.
        bool wrapTLS()                      {return TCPSocket::wrapTLS(fleece::nullslice);}
    };


}
