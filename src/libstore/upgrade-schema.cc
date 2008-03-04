#include "db.hh"
#include "hash.hh"
#include "util.hh"
#include "local-store.hh"
#include "globals.hh"

#include <iostream>


namespace nix {


Hash parseHashField(const Path & path, const string & s);


/* Upgrade from schema 4 (Nix 0.11) to schema 5 (Nix >= 0.12).  The
   old schema uses Berkeley DB, the new one stores store path
   meta-information in files. */
void upgradeStore12()
{
    printMsg(lvlError, "upgrading Nix store to new schema (this may take a while)...");

    /* Open the old Nix database and tables. */
    Database nixDB;
    nixDB.open(nixDBPath);
    
    /* dbValidPaths :: Path -> ()

       The existence of a key $p$ indicates that path $p$ is valid
       (that is, produced by a succesful build). */
    TableId dbValidPaths = nixDB.openTable("validpaths");

    /* dbReferences :: Path -> [Path]

       This table lists the outgoing file system references for each
       output path that has been built by a Nix derivation.  These are
       found by scanning the path for the hash components of input
       paths. */
    TableId dbReferences = nixDB.openTable("references");

    /* dbReferrers :: Path -> Path

       This table is just the reverse mapping of dbReferences.  This
       table can have duplicate keys, each corresponding value
       denoting a single referrer. */
    // Not needed for conversion: it's just the inverse of
    // references.
    // TableId dbReferrers = nixDB.openTable("referrers");

    /* dbDerivers :: Path -> [Path]

       This table lists the derivation used to build a path.  There
       can only be multiple such paths for fixed-output derivations
       (i.e., derivations specifying an expected hash). */
    TableId dbDerivers = nixDB.openTable("derivers");

    Paths paths;
    nixDB.enumTable(noTxn, dbValidPaths, paths);
    
    for (Paths::iterator i = paths.begin(); i != paths.end(); ++i) {
        PathSet references;
        Paths references2;
        nixDB.queryStrings(noTxn, dbReferences, *i, references2);
        references.insert(references2.begin(), references2.end());
        
        string s;
        nixDB.queryString(noTxn, dbValidPaths, *i, s);
        Hash hash =parseHashField(*i, s);
        
        Path deriver;
        nixDB.queryString(noTxn, dbDerivers, *i, deriver);
        
        registerValidPath(*i, hash, references, deriver);
        std::cerr << ".";
    }

    std::cerr << std::endl;
}


}
