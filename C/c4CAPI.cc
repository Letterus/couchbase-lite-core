//
// c4CAPI.cc
//
// Copyright © 2021 Couchbase. All rights reserved.
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

#include "c4BlobStore.hh"
#include "c4Database.hh"
#include "c4Document.hh"
#include "c4DocEnumerator.hh"
#include "c4Observer.hh"
#include "c4Query.hh"
#include "c4Replicator.hh"

#include "c4.h"
#include "c4Internal.hh"
#include "c4ExceptionUtils.hh"
#include "c4Private.h"
#include "c4QueryImpl.hh"

using namespace std;
using namespace fleece;
using namespace c4Internal;


#pragma mark - BLOBS:


bool c4blob_keyFromString(C4Slice str, C4BlobKey* outKey) noexcept {
    return C4Blob::keyFromString(str, outKey);
}


C4SliceResult c4blob_keyToString(C4BlobKey key) noexcept {
    try {
        return C4SliceResult(C4Blob::keyToString(key));
    } catchError(nullptr);
    return {};
}



C4BlobStore* c4blob_openStore(C4Slice dirPath,
                              C4DatabaseFlags flags,
                              const C4EncryptionKey *key,
                              C4Error* outError) noexcept
{
    try {
        return new C4BlobStore(dirPath, flags, key);
    } catchError(outError)
    return nullptr;
}


C4BlobStore* c4db_getBlobStore(C4Database *db, C4Error* outError) noexcept {
    try {
        return &db->getBlobStore();
    } catchError(outError)
    return nullptr;
}


void c4blob_freeStore(C4BlobStore *store) noexcept {
    delete store;
}


bool c4blob_deleteStore(C4BlobStore* store, C4Error *outError) noexcept {
    try {
        store->deleteStore();
        delete store;
        return true;
    } catchError(outError)
    return false;
}


int64_t c4blob_getSize(C4BlobStore* store, C4BlobKey key) noexcept {
    try {
        return store->getSize(key);
    } catchExceptions()
    return -1;
}


C4SliceResult c4blob_getContents(C4BlobStore* store, C4BlobKey key, C4Error* outError) noexcept {
    try {
        return C4SliceResult(store->getContents(key));
    } catchError(outError)
    return {};
}


C4StringResult c4blob_getFilePath(C4BlobStore* store, C4BlobKey key, C4Error* outError) noexcept {
    try {
        return C4StringResult(store->getFilePath(key));
    } catchError(outError)
    return {};
}


C4BlobKey c4blob_computeKey(C4Slice contents) {
    return C4Blob::computeKey(contents);
}


bool c4blob_create(C4BlobStore* store,
                   C4Slice contents,
                   const C4BlobKey *expectedKey,
                   C4BlobKey *outKey,
                   C4Error* outError) noexcept
{
    try {
        auto key = store->createBlob(contents, expectedKey);
        if (outKey)
            *outKey = key;
        return true;
    } catchError(outError)
    return false;
}


bool c4blob_delete(C4BlobStore* store, C4BlobKey key, C4Error* outError) noexcept {
    try {
        store->deleteBlob(key);
        return true;
    } catchError(outError)
    return false;
}


#pragma mark - STREAMING READS:


C4ReadStream* c4blob_openReadStream(C4BlobStore* store, C4BlobKey key, C4Error* outError) noexcept {
    try {
        return new C4ReadStream(*store, key);
    } catchError(outError)
    return nullptr;
}


size_t c4stream_read(C4ReadStream* stream, void *buffer, size_t maxBytes, C4Error* outError) noexcept {
    try {
        clearError(outError);
        return stream->read(buffer, maxBytes);
    } catchError(outError)
    return 0;
}


int64_t c4stream_getLength(C4ReadStream* stream, C4Error* outError) noexcept {
    try {
        return stream->getLength();
    } catchError(outError)
    return -1;
}


bool c4stream_seek(C4ReadStream* stream, uint64_t position, C4Error* outError) noexcept {
    try {
        stream->seek(position);
        return true;
    } catchError(outError)
    return false;
}


void c4stream_close(C4ReadStream* stream) noexcept {
    delete stream;
}


#pragma mark - STREAMING WRITES:


C4WriteStream* c4blob_openWriteStream(C4BlobStore* store, C4Error* outError) noexcept {
    try {
        return new C4WriteStream(*store);
    } catchError(outError)
    return nullptr;
}


bool c4stream_write(C4WriteStream* stream, const void *bytes, size_t length, C4Error* outError) noexcept {
    try {
        stream->write(slice(bytes, length));
        return true;
    } catchError(outError)
    return false;
}


uint64_t c4stream_bytesWritten(C4WriteStream* stream) noexcept {
    return stream->bytesWritten();
}


C4BlobKey c4stream_computeBlobKey(C4WriteStream* stream) noexcept {
    return stream->computeBlobKey();
}


bool c4stream_install(C4WriteStream* stream,
                      const C4BlobKey *expectedKey,
                      C4Error *outError) noexcept
{
    try {
        stream->install(expectedKey);
        return true;
    } catchError(outError)
    return false;
}


void c4stream_closeWriter(C4WriteStream* stream) noexcept {
    delete stream;
}


#pragma mark - DATABASE:


bool c4db_exists(C4String name, C4String inDirectory) C4API {
    return C4Database::exists(name, inDirectory);
}


bool c4key_setPassword(C4EncryptionKey *outKey, C4String password, C4EncryptionAlgorithm alg) C4API {
    if (auto key = C4EncryptionKeyFromPassword(password, alg); key) {
        *outKey = *key;
        return true;
    } else {
        return false;
    }
}


// TODO - Remove deprecated function
C4Database* c4db_open(C4Slice path,
                      const C4DatabaseConfig *configP,
                      C4Error *outError) noexcept
{
    return tryCatch<C4Database*>(outError, [=] {
        return C4Database::openAtPath(path, configP->flags, &configP->encryptionKey).detach();
    });
}


C4Database* c4db_openNamed(C4String name,
                           const C4DatabaseConfig2 *config,
                           C4Error *outError) C4API
{
    return tryCatch<C4Database*>(outError, [=] {
        return C4Database::openNamed(name, *config).detach();
    });
}


C4Database* c4db_openAgain(C4Database* db,
                           C4Error *outError) noexcept
{
    return c4db_openNamed(c4db_getName(db), c4db_getConfig2(db), outError);
}


// TODO - Remove deprecated function
bool c4db_copy(C4String sourcePath, C4String destinationPath, const C4DatabaseConfig* config,
               C4Error *error) noexcept {
    return tryCatch(error, [=] {
        C4Database::copyFileToPath(sourcePath, destinationPath, *config);
    });
}


bool c4db_copyNamed(C4String sourcePath,
                    C4String destinationName,
                    const C4DatabaseConfig2* config,
                    C4Error* error) C4API
{
    return tryCatch(error, [=] {
        C4Database::copyNamed(sourcePath, destinationName, *config);
    });
}


bool c4db_close(C4Database* database, C4Error *outError) noexcept {
    if (database == nullptr)
        return true;
    return tryCatch(outError, [=]{return database->close();});
}


bool c4db_delete(C4Database* database, C4Error *outError) noexcept {
    return tryCatch(outError, [=]{return database->closeAndDeleteFile();});
}


// TODO - Remove deprecated function
bool c4db_deleteAtPath(C4Slice dbPath, C4Error *outError) noexcept {
    if (outError)
        *outError = {};     // deleteDatabaseAtPath may return false w/o throwing an exception
    return tryCatch<bool>(outError, [=]{return C4Database::deleteAtPath(dbPath);});
}


bool c4db_deleteNamed(C4String dbName,
                      C4String inDirectory,
                      C4Error *outError) C4API
{
    if (outError)
        *outError = {};     // deleteNamed may return false w/o throwing an exception
    return tryCatch<bool>(outError, [=]{return C4Database::deleteNamed(dbName, inDirectory);});
}


// TODO - Remove deprecated function
bool c4db_compact(C4Database* database, C4Error *outError) noexcept {
    return c4db_maintenance(database, kC4Compact, outError);
}


bool c4db_maintenance(C4Database* database, C4MaintenanceType type, C4Error *outError) C4API {
    return tryCatch(outError, [=]{return database->maintenance(type);});
}


bool c4db_startHousekeeping(C4Database *db) C4API {
    return tryCatch<bool>(nullptr, [=]{
        return db->startHousekeeping();
    });
}


bool c4db_mayHaveExpiration(C4Database *db) C4API {
    return db->mayHaveExpiration();
}


C4Timestamp c4db_nextDocExpiration(C4Database *db) C4API {
    return tryCatch<uint64_t>(nullptr, [=]{
        return db->nextDocExpiration();
    });
}


int64_t c4db_purgeExpiredDocs(C4Database *db, C4Error *outError) C4API {
    int64_t count = -1;
    if (c4db_beginTransaction(db, outError)) {
        try {
            count = db->purgeExpiredDocs();
        } catchError(outError);
        if (!c4db_endTransaction(db, (count > 0), outError))
            count = -1;
    }
    return count;
}


bool c4db_rekey(C4Database* database, const C4EncryptionKey *newKey, C4Error *outError) noexcept {
    return tryCatch(outError, [=]{return database->rekey(newKey);});
}


C4String c4db_getName(C4Database *database) C4API {
    return slice(database->getName());
}

C4SliceResult c4db_getPath(C4Database *database) noexcept {
    return C4SliceResult(database->path());
}


const C4DatabaseConfig* c4db_getConfig(C4Database *database) noexcept {
    return &database->getConfigV1();
}


const C4DatabaseConfig2* c4db_getConfig2(C4Database *database) noexcept {
    return &database->getConfig();
}


uint64_t c4db_getDocumentCount(C4Database* database) noexcept {
    return tryCatch<uint64_t>(nullptr, [=]{return database->getDocumentCount();});
}


C4SequenceNumber c4db_getLastSequence(C4Database* database) noexcept {
    return tryCatch<sequence_t>(nullptr, [=]{return database->getLastSequence();});
}


bool c4db_getUUIDs(C4Database* database, C4UUID *publicUUID, C4UUID *privateUUID,
                   C4Error *outError) noexcept
{
    return tryCatch(outError, [&]{
        if (publicUUID)
            *publicUUID = database->publicUUID();
        if (privateUUID)
            *privateUUID = database->privateUUID();
    });
}


C4StringResult c4db_getPeerID(C4Database* database) C4API {
    return tryCatch<C4StringResult>(nullptr, [&]{
        return C4StringResult(database->getPeerID());
    });
}


C4ExtraInfo c4db_getExtraInfo(C4Database *database) C4API {
    return database->extraInfo;
}

void c4db_setExtraInfo(C4Database *database, C4ExtraInfo x) C4API {
    database->extraInfo = x;
}


bool c4db_isInTransaction(C4Database* database) noexcept {
    return database->isInTransaction();
}


bool c4db_beginTransaction(C4Database* database,
                           C4Error *outError) noexcept
{
    return tryCatch(outError, [=]{database->beginTransaction();});
}

bool c4db_endTransaction(C4Database* database,
                         bool commit,
                         C4Error *outError) noexcept
{
    return tryCatch(outError, [=]{database->endTransaction(commit);});
}


void c4db_lock(C4Database *db) C4API {
    db->lockClientMutex();
}


void c4db_unlock(C4Database *db) C4API {
    db->unlockClientMutex();
}


bool c4db_purgeDoc(C4Database *database, C4Slice docID, C4Error *outError) noexcept {
    try {
        if (database->purgeDoc(docID))
            return true;
        else
            c4error_return(LiteCoreDomain, kC4ErrorNotFound, {}, outError);
    } catchError(outError)
    return false;
}


bool c4_shutdown(C4Error *outError) noexcept {
    return tryCatch(outError, [] {
        C4Database::shutdownLiteCore();
    });
}


C4SliceResult c4db_rawQuery(C4Database *database, C4String query, C4Error *outError) noexcept {
    try {
        return C4SliceResult(database->rawQuery(query));
    } catchError(outError)
    return {};
}
// LCOV_EXCL_STOP


bool c4db_findDocAncestors(C4Database *database,
                           unsigned numDocs,
                           unsigned maxAncestors,
                           bool requireBodies,
                           C4RemoteID remoteDBID,
                           const C4String docIDs[], const C4String revIDs[],
                           C4StringResult ancestors[],
                           C4Error *outError) C4API
{
    return tryCatch(outError, [&]{
        vector<slice> vecDocIDs((const slice*)&docIDs[0], (const slice*)&docIDs[numDocs]);
        vector<slice> vecRevIDs((const slice*)&revIDs[0], (const slice*)&revIDs[numDocs]);
        auto vecAncestors = database->findDocAncestors(vecDocIDs, vecRevIDs,
                                                       maxAncestors, requireBodies,
                                                       remoteDBID);
        for (unsigned i = 0; i < numDocs; ++i)
            ancestors[i] = C4SliceResult(vecAncestors[i]);
    });
}


void c4raw_free(C4RawDocument* rawDoc) noexcept {
    if (rawDoc) {
        ((slice)rawDoc->key).free();
        ((slice)rawDoc->meta).free();
        ((slice)rawDoc->body).free();
        delete rawDoc;
    }
}


C4RawDocument* c4raw_get(C4Database* database,
                         C4Slice storeName,
                         C4Slice key,
                         C4Error *outError) noexcept
{
    return tryCatch<C4RawDocument*>(outError, [&]{
        C4RawDocument *rawDoc = nullptr;
        database->getRawDocument(storeName, key, [&rawDoc](C4RawDocument *r) {
            if (r) {
                rawDoc = new C4RawDocument{slice(r->key).copy(),
                                           slice(r->meta).copy(),
                                           slice(r->body).copy()};
            }
        });
        if (!rawDoc)
            c4error_return(LiteCoreDomain, kC4ErrorNotFound, {}, outError);
        return rawDoc;
    });
}


bool c4raw_put(C4Database* database,
               C4Slice storeName,
               C4Slice key,
               C4Slice meta,
               C4Slice body,
               C4Error *outError) noexcept
{
    return tryCatch(outError, [=]{
        database->putRawDocument(storeName, {key, meta, body});
    });
}


bool c4db_createIndex(C4Database *database,
                      C4Slice name,
                      C4Slice indexSpecJSON,
                      C4IndexType indexType,
                      const C4IndexOptions *indexOptions,
                      C4Error *outError) noexcept
{
    return tryCatch(outError, [&]{
        database->createIndex(name, indexSpecJSON, indexType, indexOptions);
    });
}


bool c4db_deleteIndex(C4Database *database,
                      C4Slice name,
                      C4Error *outError) noexcept
{
    return tryCatch(outError, [&]{
        database->deleteIndex(name);
    });
}

C4SliceResult c4db_getIndexes(C4Database* database, C4Error* outError) noexcept {
    return tryCatch<C4SliceResult>(outError, [&]{
        return C4SliceResult(database->getIndexes());
    });
}


C4SliceResult c4db_getIndexesInfo(C4Database* database, C4Error* outError) noexcept {
    return tryCatch<C4SliceResult>(outError, [&]{
        return C4SliceResult(database->getIndexesInfo());
    });
}


C4SliceResult c4db_getIndexRows(C4Database* database, C4String indexName, C4Error* outError) noexcept {
    return tryCatch<C4SliceResult>(outError, [&]{
        return C4SliceResult(database->getIndexRows(indexName));
    });
}


C4StringResult c4db_getCookies(C4Database *db,
                               C4Address request,
                               C4Error *outError) C4API
{
    return tryCatch<C4StringResult>(outError, [=]() {
        return C4StringResult(db->getCookies(request));
    });
}


bool c4db_setCookie(C4Database *db,
                    C4String setCookieHeader,
                    C4String fromHost,
                    C4String fromPath,
                    C4Error *outError) C4API
{
    return tryCatch<bool>(outError, [=]() {
        if (db->setCookie(setCookieHeader, fromHost, fromPath))
            return true;
        c4error_return(LiteCoreDomain, kC4ErrorInvalidParameter, C4STR("Invalid cookie"), outError);
        return false;
    });
}


void c4db_clearCookies(C4Database *db) C4API {
    tryCatch(nullptr, [db]() {
        db->clearCookies();
    });
}


#pragma mark - DOCUMENT:




C4Document* c4doc_retain(C4Document *doc) noexcept {
    return retain(doc);
}


void c4doc_release(C4Document *doc) noexcept {
   release(doc);
}


C4Document* c4db_getDoc(C4Database *database,
                       C4Slice docID,
                       bool mustExist,
                       C4DocContentLevel content,
                       C4Error *outError) noexcept
{
    return tryCatch<C4Document*>(outError, [&]{
        Retained<C4Document> doc = database->getDocument(docID, mustExist, content);
        if (!doc)
            c4error_return(LiteCoreDomain, kC4ErrorNotFound, {}, outError);
        return move(doc).detach();
    });
}


C4Document* c4doc_get(C4Database *database,
                      C4Slice docID,
                      bool mustExist,
                      C4Error *outError) noexcept
{
    return c4db_getDoc(database, docID, mustExist, kDocGetCurrentRev, outError);
}


C4Document* c4doc_getBySequence(C4Database *database,
                                C4SequenceNumber sequence,
                                C4Error *outError) noexcept
{
    return tryCatch<C4Document*>(outError, [&]{
        auto doc = database->getDocumentBySequence(sequence);
        if (!doc)
            c4error_return(LiteCoreDomain, kC4ErrorNotFound, {}, outError);
        return move(doc).detach();
    });
}


bool c4doc_setExpiration(C4Database *db, C4Slice docId, C4Timestamp timestamp, C4Error *outError) C4API {
    return tryCatch<bool>(outError, [=]{
        if (db->setExpiration(docId, timestamp))
            return true;
        c4error_return(LiteCoreDomain, kC4ErrorNotFound, {}, outError);
        return false;
    });
}


C4Timestamp c4doc_getExpiration(C4Database *db, C4Slice docID, C4Error *outError) C4API {
    C4Timestamp expiration = -1;
    tryCatch(outError, [&]{
        expiration = db->getExpiration(docID);
    });
    return expiration;
}


bool c4doc_selectRevision(C4Document* doc,
                          C4Slice revID,
                          bool withBody,
                          C4Error *outError) noexcept
{
    return tryCatch<bool>(outError, [&]{
        if (doc->selectRevision(revID, withBody))
            return true;
        c4error_return(LiteCoreDomain, kC4ErrorNotFound, {}, outError);
        return false;
    });
}


bool c4doc_selectCurrentRevision(C4Document* doc) noexcept {
    return doc->selectCurrentRevision();
}


bool c4doc_loadRevisionBody(C4Document* doc, C4Error *outError) noexcept {
    return tryCatch<bool>(outError, [&]{
        if (doc->loadRevisionBody())
            return true;
        c4error_return(LiteCoreDomain, kC4ErrorNotFound, {}, outError);
        return false;
    });
}


bool c4doc_hasRevisionBody(C4Document* doc) noexcept {
    return tryCatch<bool>(nullptr, [=]{return doc->hasRevisionBody();});
}


C4Slice c4doc_getRevisionBody(C4Document* doc) C4API {
    return doc->getRevisionBody();
}


C4SliceResult c4doc_getSelectedRevIDGlobalForm(C4Document* doc) C4API {
    return C4SliceResult(doc->getSelectedRevIDGlobalForm());
}


C4SliceResult c4doc_getRevisionHistory(C4Document* doc,
                                       unsigned maxRevs,
                                       const C4String backToRevs[],
                                       unsigned backToRevsCount) C4API {
    return C4SliceResult(doc->getRevisionHistory(maxRevs, backToRevs, backToRevsCount));
}


bool c4doc_selectParentRevision(C4Document* doc) noexcept {
    return doc->selectParentRevision();
}


bool c4doc_selectNextRevision(C4Document* doc) noexcept {
    return tryCatch<bool>(nullptr, [=]{return doc->selectNextRevision();});
}


bool c4doc_selectNextLeafRevision(C4Document* doc,
                                  bool includeDeleted,
                                  bool withBody,
                                  C4Error *outError) noexcept
{
    return tryCatch<bool>(outError, [&]{
        if (doc->selectNextLeafRevision(includeDeleted))
            return true;
        clearError(outError); // normal failure
        return false;
    });
}


bool c4doc_selectCommonAncestorRevision(C4Document* doc, C4String rev1, C4String rev2) noexcept {
    return tryCatch<bool>(nullptr, [&]{
        return doc->selectCommonAncestorRevision(rev1, rev2);
    });
}


bool c4doc_removeRevisionBody(C4Document* doc) noexcept {
    return doc->removeRevisionBody();
}


// this function is probably unused; remove it if so
int32_t c4doc_purgeRevision(C4Document *doc,
                            C4Slice revID,
                            C4Error *outError) noexcept
{
    try {
        return doc->purgeRevision(revID);
    } catchError(outError)
    return -1;
}


C4RemoteID c4db_getRemoteDBID(C4Database *db, C4String remoteAddress, bool canCreate,
                              C4Error *outError) C4API
{
    return tryCatch<C4RemoteID>(outError, [&]{
        return db->getRemoteDBID(remoteAddress, canCreate);
    });
}


C4SliceResult c4db_getRemoteDBAddress(C4Database *db, C4RemoteID remoteID) C4API {
    return tryCatch<C4SliceResult>(nullptr, [&]{
        return C4SliceResult(db->getRemoteDBAddress(remoteID));
    });
}


C4SliceResult c4doc_getRemoteAncestor(C4Document *doc, C4RemoteID remoteDatabase) C4API {
    return tryCatch<C4SliceResult>(nullptr, [&]{
        return C4SliceResult(doc->getRemoteAncestor(remoteDatabase));
    });
}


bool c4doc_setRemoteAncestor(C4Document *doc, C4RemoteID remoteDatabase, C4String revID,
                             C4Error *outError) C4API
{
    return tryCatch<bool>(outError, [&]{
        doc->setRemoteAncestor(remoteDatabase, revID);
        return true;
    });
}


bool c4db_markSynced(C4Database *database,
                     C4String docID,
                     C4String revID,
                     C4SequenceNumber sequence,
                     C4RemoteID remoteID,
                     C4Error *outError) noexcept
{
    return tryCatch<bool>(outError, [&]{
        return database->markDocumentSynced(docID, revID, sequence, remoteID);
    });
}


char* c4doc_generateID(char *docID, size_t bufferSize) noexcept {
    return C4Document::generateID(docID, bufferSize);
}


C4Document* c4doc_put(C4Database *database,
                      const C4DocPutRequest *rq,
                      size_t *outCommonAncestorIndex,
                      C4Error *outError) noexcept
{
    return tryCatch<C4Document*>(outError, [&]{
        return database->putDocument(*rq, outCommonAncestorIndex, outError).detach();
    });
}


C4Document* c4doc_create(C4Database *database,
                         C4String docID,
                         C4Slice revBody,
                         C4RevisionFlags revFlags,
                         C4Error *outError) noexcept
{
    return tryCatch<C4Document*>(outError, [&]{
        return database->createDocument(docID, revBody, revFlags, outError).detach();
    });
}


C4Document* c4doc_update(C4Document *doc,
                         C4Slice revBody,
                         C4RevisionFlags revFlags,
                         C4Error *outError) noexcept
{
    return tryCatch<C4Document*>(outError, [&]{
        Retained<C4Document> updated = doc->update(revBody, revFlags);
        if (!updated)
            c4error_return(LiteCoreDomain, kC4ErrorConflict, nullslice, outError);
        return move(updated).detach();
    });
}


bool c4doc_resolveConflict2(C4Document *doc,
                            C4String winningRevID,
                            C4String losingRevID,
                            FLDict mergedProperties,
                            C4RevisionFlags mergedFlags,
                            C4Error *outError) noexcept
{
    return tryCatch(outError, [&]{
        doc->resolveConflict(winningRevID, losingRevID, mergedProperties, mergedFlags);
    });
}


bool c4doc_resolveConflict(C4Document *doc,
                           C4String winningRevID,
                           C4String losingRevID,
                           C4Slice mergedBody,
                           C4RevisionFlags mergedFlags,
                           C4Error *outError) noexcept
{
    return tryCatch(outError, [&]{
        doc->resolveConflict(winningRevID, losingRevID, mergedBody, mergedFlags);
    });
}


bool c4doc_save(C4Document *doc,
                uint32_t maxRevTreeDepth,
                C4Error *outError) noexcept
{
    try {
        if (doc->save(maxRevTreeDepth))
            return true;
        c4error_return(LiteCoreDomain, kC4ErrorConflict, nullslice, outError);
        return false;
    } catchError(outError)
    return false;
}


/// Returns true if the two ASCII revIDs are equal (though they may not be byte-for-byte equal.)
bool c4rev_equal(C4Slice rev1, C4Slice rev2) C4API {
    return C4Document::equalRevIDs(rev1, rev2);
}


unsigned c4rev_getGeneration(C4Slice revID) C4API {
    return C4Document::getRevIDGeneration(revID);
}


C4RevisionFlags c4rev_flagsFromDocFlags(C4DocumentFlags docFlags) C4API {
    return C4Document::revisionFlagsFromDocFlags(docFlags);
}


FLDict c4doc_getProperties(C4Document* doc) C4API {
    return doc->getProperties();
}


C4Document* c4doc_containingValue(FLValue value) {
    return C4Document::containingValue(value);
}


FLEncoder c4db_createFleeceEncoder(C4Database* db) noexcept {
    return db->createFleeceEncoder();
}


FLEncoder c4db_getSharedFleeceEncoder(C4Database* db) noexcept {
    return db->getSharedFleeceEncoder();
}


C4SliceResult c4db_encodeJSON(C4Database *db, C4Slice jsonData, C4Error *outError) noexcept {
    return tryCatch<C4SliceResult>(outError, [&]{
        return C4SliceResult(db->encodeJSON(jsonData));
    });
}


C4SliceResult c4doc_bodyAsJSON(C4Document *doc, bool canonical, C4Error *outError) noexcept {
    return tryCatch<C4SliceResult>(outError, [&]{
        return C4SliceResult(doc->bodyAsJSON(canonical));
    });
}


FLSharedKeys c4db_getFLSharedKeys(C4Database *db) noexcept {
    return db->getFLSharedKeys();
}


bool c4doc_isOldMetaProperty(C4String prop) noexcept {
    return C4Document::isOldMetaProperty(prop);
}


bool c4doc_hasOldMetaProperties(FLDict doc) noexcept {
    return C4Document::hasOldMetaProperties(doc);
}


bool c4doc_getDictBlobKey(FLDict dict, C4BlobKey *outKey) {
    return C4Blob::getKey(dict, *outKey);
}


bool c4doc_dictIsBlob(FLDict dict, C4BlobKey *outKey) C4API {
    Assert(outKey);
    return C4Blob::isBlob(dict, *outKey);
}


C4SliceResult c4doc_getBlobData(FLDict flDict, C4BlobStore *blobStore, C4Error *outError) C4API {
    return tryCatch<C4SliceResult>(outError, [&]{
        return C4SliceResult(blobStore->getBlobData(flDict));
    });
}


bool c4doc_dictContainsBlobs(FLDict dict) noexcept {
    return C4Document::containsBlobs(dict);
}


bool c4doc_blobIsCompressible(FLDict blobDict) {
    return C4Blob::isCompressible(blobDict);
}


C4SliceResult c4doc_encodeStrippingOldMetaProperties(FLDict doc, FLSharedKeys sk, C4Error *outError) noexcept {
    return tryCatch<C4SliceResult>(outError, [&]{
        return C4SliceResult(C4Document::encodeStrippingOldMetaProperties(doc, sk));
    });
}


#pragma mark - DOC ENUMERATOR:


void c4enum_close(C4DocEnumerator *e) noexcept {
    if (e)
        e->close();
}

void c4enum_free(C4DocEnumerator *e) noexcept {
    delete e;
}


C4DocEnumerator* c4db_enumerateChanges(C4Database *database,
                                       C4SequenceNumber since,
                                       const C4EnumeratorOptions *c4options,
                                       C4Error *outError) noexcept
{
    return tryCatch<C4DocEnumerator*>(outError, [&]{
        return new C4DocEnumerator(database, since,
                                   c4options ? *c4options : kC4DefaultEnumeratorOptions);
    });
}


C4DocEnumerator* c4db_enumerateAllDocs(C4Database *database,
                                       const C4EnumeratorOptions *c4options,
                                       C4Error *outError) noexcept
{
    return tryCatch<C4DocEnumerator*>(outError, [&]{
        return new C4DocEnumerator(database,
                                   c4options ? *c4options : kC4DefaultEnumeratorOptions);
    });
}


bool c4enum_next(C4DocEnumerator *e, C4Error *outError) noexcept {
    return tryCatch<bool>(outError, [&]{
        if (e->next())
            return true;
        clearError(outError);      // end of iteration is not an error
        return false;
    });
}


bool c4enum_getDocumentInfo(C4DocEnumerator *e, C4DocumentInfo *outInfo) noexcept {
    return e->getDocumentInfo(*outInfo);
}


C4Document* c4enum_getDocument(C4DocEnumerator *e, C4Error *outError) noexcept {
    return tryCatch<C4Document*>(outError, [&]{
        Retained<C4Document> doc = e->getDocument();
        if (!doc)
            clearError(outError);      // end of iteration is not an error
        return move(doc).detach();
    });
}


#pragma mark - OBSERVERS:


C4DatabaseObserver* c4dbobs_create(C4Database *db,
                                   C4DatabaseObserverCallback callback,
                                   void *context) noexcept
{
    return C4DatabaseObserver::create(db, [=](C4DatabaseObserver *obs) {
        callback(obs, context);
    }).release();
}


uint32_t c4dbobs_getChanges(C4DatabaseObserver *obs,
                            C4DatabaseChange outChanges[],
                            uint32_t maxChanges,
                            bool *outExternal) noexcept
{
    static_assert(sizeof(C4DatabaseChange) == sizeof(C4DatabaseObserver::Change),
                  "C4DatabaseChange doesn't match C4DatabaseObserver::Change");
    return tryCatch<uint32_t>(nullptr, [&]{
        memset(outChanges, 0, maxChanges * sizeof(C4DatabaseChange));
        return obs->getChanges((C4DatabaseObserver::Change*)outChanges,
                               maxChanges, outExternal);
        // This is slightly sketchy because C4DatabaseObserver::Change contains alloc_slices, whereas
        // C4DatabaseChange contains slices. The result is that the docID and revID memory will be
        // temporarily leaked, since the alloc_slice destructors won't be called.
        // For this purpose we have c4dbobs_releaseChanges(), which does the same sleight of hand
        // on the array but explicitly destructs each Change object, ensuring its alloc_slices are
        // destructed and the backing store's ref-count goes back to what it was originally.
    });
}


void c4dbobs_releaseChanges(C4DatabaseChange changes[], uint32_t numChanges) noexcept {
    for (uint32_t i = 0; i < numChanges; ++i) {
        auto &change = (C4DatabaseObserver::Change&)changes[i];
        change.~Change();
    }
}


void c4dbobs_free(C4DatabaseObserver* obs) noexcept {
    delete obs;
}


C4DocumentObserver* c4docobs_create(C4Database *db,
                                    C4Slice docID,
                                    C4DocumentObserverCallback callback,
                                    void *context) noexcept
{
    return tryCatch<unique_ptr<C4DocumentObserver>>(nullptr, [&]{
        auto fn = [=](C4DocumentObserver *obs, fleece::slice docID, C4SequenceNumber seq) {
            callback(obs, docID, seq, context);
        };
        return C4DocumentObserver::create(db, docID, fn);
    }).release();
}


void c4docobs_free(C4DocumentObserver* obs) noexcept {
    delete obs;
}


#pragma mark - QUERY:


C4Query* c4query_new2(C4Database *database,
                      C4QueryLanguage language,
                      C4Slice expression,
                      int *outErrorPos,
                      C4Error *outError) noexcept
{
    if (outErrorPos)
        *outErrorPos = -1;
    return tryCatch<C4Query*>(outError, [&]{
        C4Query *query = database->newQuery(language, expression, outErrorPos).detach();
        if (!query)
            c4error_return(LiteCoreDomain, kC4ErrorInvalidQuery, {}, outError);
        return query;
    });
}


// deprecated
C4Query* c4query_new(C4Database *database, C4String expression, C4Error *error) C4API {
    return c4query_new2(database, kC4JSONQuery, expression, nullptr, error);
}


unsigned c4query_columnCount(C4Query *query) noexcept {
    return query->columnCount();
}


FLString c4query_columnTitle(C4Query *query, unsigned column) C4API {
    return query->columnTitle(column);
}


void c4query_setParameters(C4Query *query, C4String encodedParameters) C4API {
    query->setParameters(encodedParameters);
}


C4QueryEnumerator* c4query_run(C4Query *query,
                               const C4QueryOptions *c4options,
                               C4Slice encodedParameters,
                               C4Error *outError) noexcept
{
    return tryCatch<C4QueryEnumerator*>(outError, [&]{
        return query->createEnumerator(c4options, encodedParameters);
    });
}


C4StringResult c4query_explain(C4Query *query) noexcept {
    return tryCatch<C4StringResult>(nullptr, [&]{
        return C4StringResult(query->explain());
    });
}


C4SliceResult c4query_fullTextMatched(C4Query *query,
                                      const C4FullTextMatch *term,
                                      C4Error *outError) noexcept
{
    return tryCatch<C4SliceResult>(outError, [&]{
        return C4SliceResult(query->fullTextMatched(*term));
    });
}


#pragma mark - QUERY ENUMERATOR API:


bool c4queryenum_next(C4QueryEnumerator *e,
                      C4Error *outError) noexcept
{
    return tryCatch<bool>(outError, [&]{
        if (asInternal(e)->next())
            return true;
        clearError(outError);      // end of iteration is not an error
        return false;
    });
}


bool c4queryenum_seek(C4QueryEnumerator *e,
                      int64_t rowIndex,
                      C4Error *outError) noexcept
{
    return tryCatch<bool>(outError, [&]{
        asInternal(e)->seek(rowIndex);
        return true;
    });
}


int64_t c4queryenum_getRowCount(C4QueryEnumerator *e,
                                 C4Error *outError) noexcept
{
    try {
        return asInternal(e)->getRowCount();
    } catchError(outError)
    return -1;
}



C4QueryEnumerator* c4queryenum_refresh(C4QueryEnumerator *e,
                                       C4Error *outError) noexcept
{
    return tryCatch<C4QueryEnumerator*>(outError, [&]{
        clearError(outError);
        return asInternal(e)->refresh();
    });
}


C4QueryEnumerator* c4queryenum_retain(C4QueryEnumerator *e) C4API {
    return retain(asInternal(e));
}


void c4queryenum_close(C4QueryEnumerator *e) noexcept {
    if (e) {
        asInternal(e)->close();
    }
}

void c4queryenum_release(C4QueryEnumerator *e) noexcept {
    release(asInternal(e));
}


#pragma mark - QUERY OBSERVER API:


C4QueryObserver* c4queryobs_create(C4Query *query, C4QueryObserverCallback cb, void *ctx) C4API {
    auto fn = [cb,ctx](C4QueryObserver *obs) {
        cb(obs, obs->query(), ctx);
    };
    return new C4QueryObserverImpl(query, fn);
}

void c4queryobs_setEnabled(C4QueryObserver *obs, bool enabled) C4API {
    obs->setEnabled(enabled);
}

void c4queryobs_free(C4QueryObserver* obs) C4API {
    delete obs;
}

C4QueryEnumerator* c4queryobs_getEnumerator(C4QueryObserver *obs,
                                            bool forget,
                                            C4Error *outError) C4API
{
    return asInternal(obs)->getEnumeratorImpl(forget, outError).detach();
}

