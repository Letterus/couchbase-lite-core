//
//  c4Database.hh
//
// Copyright (c) 2019 Couchbase, Inc All rights reserved.
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
#include "Database.hh"
#include "c4Base.h"


// This is the struct that's forward-declared in the public c4Database.h
struct C4Database : public c4Internal::Database {
    C4Database(const FilePath &path, C4DatabaseConfig config)
    :Database(path, config) { }

    C4ExtraInfo extraInfo { };

private:
    ~C4Database();
};


static inline C4Database* external(c4Internal::Database *db)    {return (C4Database*)db;}
