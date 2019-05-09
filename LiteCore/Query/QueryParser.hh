//
// QueryParser.hh
//
// Copyright (c) 2016 Couchbase, Inc All rights reserved.
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
#include "Base.hh"
#include "UnicodeCollator.hh"
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace fleece::impl {
    class Array;
    class ArrayIterator;
    class Dict;
    class Path;
    class Value;
}

namespace litecore {


    /** Translates queries from our JSON schema (actually Fleece) into SQL runnable by SQLite.
        https://github.com/couchbase/couchbase-lite-core/wiki/JSON-Query-Schema */
    class QueryParser {
    public:
        using string = std::string;
        using string_view = std::string_view;
        template <class T> using set = std::set<T>;
        template <class T> using vector = std::vector<T>;
        using Array = fleece::impl::Array;
        using ArrayIterator = fleece::impl::ArrayIterator;
        using Value = fleece::impl::Value;

        // Default SQL column name containing document body
        static constexpr const char* kBodyColumnName = "body";


        /** Delegate knows about the naming & existence of tables.
            Implemented by SQLiteDataFile; this interface is to keep the QueryParser isolated from
            such details and make it easier to unit-test. */
        class Delegate {
        public:
            virtual ~Delegate() =default;
            virtual bool tableExists(const string &tableName) const =0;
            virtual string collectionTableName(const string &collection) const =0;
            virtual string FTSTableName(const string &onTable, const string &property) const =0;
            virtual string unnestedTableName(const string &onTable, const string &property) const =0;
#ifdef COUCHBASE_ENTERPRISE
            virtual string predictiveTableName(const string &onTable, const string &property) const =0;
#endif
        };


        QueryParser(const Delegate &delegate,
                    const string& defaultTableName)
        :_delegate(delegate)
        ,_defaultTableName(defaultTableName)
        { }

        void setBodyColumnName(const string &name)              {_bodyColumnName = name;}

        void parse(const Value*);
        void parseJSON(slice);

        void parseJustExpression(const Value *expression);

        void writeCreateIndex(const string &name,
                              const string &onTableName,
                              ArrayIterator &whatExpressions,
                              const Array *whereClause,
                              bool isUnnestedTable);

        string SQL()  const                                     {return _sql.str();}

        const set<string>& parameters()                         {return _parameters;}
        const set<string>& collectionTablesUsed() const         {return _kvTables;}
        const vector<string>& ftsTablesUsed() const             {return _ftsTables;}
        unsigned firstCustomResultColumn() const                {return _1stCustomResultCol;}
        const vector<string>& columnTitles() const              {return _columnTitles;}

        bool isAggregateQuery() const                           {return _isAggregateQuery;}
        bool usesExpiration() const                             {return _checkedExpiration;}

        string expressionSQL(const Value*);
        string whereClauseSQL(const Value*, string_view dbAlias);
        string eachExpressionSQL(const Value*);
        string FTSExpressionSQL(const Value*);
        static string FTSColumnName(const Value *expression);
        string unnestedTableName(const Value *key) const;
        string predictiveIdentifier(const Value *) const;
        string predictiveTableName(const Value *) const;

    private:
        template <class T, class U> using map = std::map<T,U>;
        using stringstream = std::stringstream;
        using Dict = fleece::impl::Dict;
        using Path = fleece::impl::Path;

        enum aliasType {
            kDBAlias,                   // Primary FROM collection
            kJoinAlias,                 // A JOINed collection
            kResultAlias,               // A named query result value ("SELECT ___ AS x")
            kUnnestVirtualTableAlias,   // An UNNEST implemented as a virtual table
            kUnnestTableAlias           // An UNNEST implemented as a materialized table
        };

        struct aliasInfo {
            aliasType type;
            string    tableName;
        };

        using AliasMap = map<string, aliasInfo>;

        QueryParser(const QueryParser *qp)
        :QueryParser(qp->_delegate, qp->_defaultTableName)
        {
            _bodyColumnName = qp->_bodyColumnName;
        }

        struct Operation;
        static const Operation kOperationList[];
        static const Operation kOuterOperation, kArgListOperation, kColumnListOperation,
                               kResultListOperation, kExpressionListOperation,
                               kHighPrecedenceOperation;
        struct JoinedOperations;
        static const JoinedOperations kJoinedOperationsList[];

        QueryParser(const QueryParser &qp) =delete;
        QueryParser& operator=(const QueryParser&) =delete;

        void reset();
        void parseNode(const Value*);
        void parseOpNode(const Array*);
        void handleOperation(const Operation*, slice actualOperator, ArrayIterator& operands);
        void parseStringLiteral(slice str);

        void writeSelect(const Dict *dict);
        void writeSelect(const Value *where, const Dict *operands);
        unsigned writeSelectListClause(const Dict *operands, slice key, const char *sql,
                                       bool aggregatesOK =false);

        void writeWhereClause(const Value *where);

        void addAlias(const string &alias, aliasType, const string &tableName);
        struct FromAttributes;
        FromAttributes parseFromEntry(const Value *value);
        void parseFromClause(const Value *from);
        void writeFromClause(const Value *from);
        int parseJoinType(slice);
        bool writeOrderOrLimitClause(const Dict *operands, slice jsonKey, const char *keyword);

        void prefixOp(slice, ArrayIterator&);
        void postfixOp(slice, ArrayIterator&);
        void infixOp(slice, ArrayIterator&);
        void resultOp(slice, ArrayIterator&);
        void arrayLiteralOp(slice, ArrayIterator&);
        void betweenOp(slice, ArrayIterator&);
        void existsOp(slice, ArrayIterator&);
        void collateOp(slice, ArrayIterator&);
        void concatOp(slice, ArrayIterator&);
        void inOp(slice, ArrayIterator&);
        void likeOp(slice, ArrayIterator&);
        void matchOp(slice, ArrayIterator&);
        void anyEveryOp(slice, ArrayIterator&);
        void parameterOp(slice, ArrayIterator&);
        void propertyOp(slice, ArrayIterator&);
        void objectPropertyOp(slice, ArrayIterator&);
        void blobOp(slice, ArrayIterator&);
        void variableOp(slice, ArrayIterator&);
        void missingOp(slice, ArrayIterator&);
        void caseOp(slice, ArrayIterator&);
        void selectOp(slice, ArrayIterator&);
        void metaOp(slice, ArrayIterator&);
        void fallbackOp(slice, ArrayIterator&);

        void functionOp(slice, ArrayIterator&);

        void writeDictLiteral(const Dict*);
        bool writeNestedPropertyOpIfAny(slice fnName, ArrayIterator &operands);
        void writePropertyGetter(slice fn, Path &&property, const Value *param =nullptr);
        void writeFunctionGetter(slice fn, const Value *source, const Value *param =nullptr);
        void writeUnnestPropertyGetter(slice fn, Path &property, const string &alias, aliasType);
        void writeEachExpression(Path &&property);
        void writeEachExpression(const Value *arrayExpr);
        void writeArgList(ArrayIterator& operands);
        void writeColumnList(ArrayIterator& operands);
        void writeResultColumn(const Value*);
        void writeCollation();
        void parseCollatableNode(const Value*);
        void writeMetaProperty(slice fn, const string &tablePrefix, slice property);

        void parseJoin(const Dict*);

        unsigned findFTSProperties(const Value *root);
        void findPredictionCalls(const Value *root);
        const string& indexJoinTableAlias(const string &key, const char *aliasPrefix =nullptr);
        const string&  FTSJoinTableAlias(const Value *matchLHS, bool canAdd =false);
        const string&  predictiveJoinTableAlias(const Value *expr, bool canAdd =false);
        string FTSTableName(const Value *key) const;
        string expressionIdentifier(const Array *expression, unsigned maxItems =0) const;
        void findPredictiveJoins(const Value *node, vector<string> &joins);
        bool writeIndexedPrediction(const Array *node);

        void writeMetaPropertyGetter(slice metaKey, const string& dbAlias);
        AliasMap::const_iterator verifyDbAlias(Path &property);
        bool optimizeMetaKeyExtraction(ArrayIterator&);

        const Delegate& _delegate;               // delegate object (SQLiteKeyStore)
        string _defaultTableName;                // Name of the default table to use
        string _bodyColumnName = kBodyColumnName;// Column holding doc bodies
        AliasMap _aliases;                       // "AS..." aliases for db/joins/unnests
        string _dbAlias;                         // Alias of the main collection, "_doc" by default
        bool _propertiesUseSourcePrefix {false}; // Must properties include alias as prefix?
        vector<string> _columnTitles;            // Pretty names of result columns
        stringstream _sql;                       // The SQL being generated
        const Value* _curNode;                   // Current node being parsed
        vector<const Operation*> _context;       // Parser stack
        set<string> _parameters;                 // Plug-in "$" parameters found in parsing
        set<string> _variables;                  // Active variables, inside ANY/EVERY exprs
        map<string, string> _indexJoinTables;    // index table name --> alias
        set<string> _kvTables;                   // Collection tables referenced in this query
        vector<string> _ftsTables;               // FTS virtual tables being used
        unsigned _1stCustomResultCol {0};        // Index of 1st result after _baseResultColumns
        bool _aggregatesOK {false};              // Are aggregate fns OK to call?
        bool _isAggregateQuery {false};          // Is this an aggregate query?
        bool _checkedExpiration {false};         // Has query accessed _expiration meta-property?
        Collation _collation;                    // Collation in use during parse
        bool _collationUsed {true};              // Emitted SQL "COLLATION" yet?
        bool _functionWantsCollation {false};    // Current fn wants collation param in its arg list
    };

}
