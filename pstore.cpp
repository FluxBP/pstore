/*

  PermaStore

  A simple contract that allows the storage of arbitrary amounts of binary data
  into an Antelope blockchain's RAM.

  Users ("owner") can create "files" (multipart binary files) that share the same
  global namespace of Antelope 64-bit names. File names are first-come, first-serve.

  Once a file is created by its owner account, the owner can set data nodes (parts)
  on it, starting from node 0 and onwards (data node IDs must be contiguous).

  Once the data upload is done, the file can be flagged as published (ready).
  Published files can also be set to immutable.

  Notes:
  
  A good node size limit is 64,000 bytes, given that some Linux systems have 128kb
  command-line limits. You will need to post the entire data for a node on the
  command line as a hexadecimal text string when using cleos, which will bloat it
  to 128,000 bytes, leaving a good room of 3,072 bytes for the rest of the cleos
  command-line content.

  In any case, too-large blocks on the network are kind of bad, and the overhead
  of splitting at 64KB or e.g. 1MB (a common Antelope network transaction size
  limit in 2023) is roughtly the same.

*/

#include <eosio/eosio.hpp>

using namespace eosio;

using namespace std;

class [[eosio::contract]] pstore : public contract {
public:
  using contract::contract;

  // File table is scoped by file name, record is a singleton.
  struct [[eosio::table]] file {
    name                    owner;      // account that controls the file (0 == no one / immutable)
    uint32_t                top;        // first empty node after last data node
    bool                    published;  // if the file is ready for use
    uint64_t primary_key() const { return 0; }
  };

  typedef eosio::multi_index< "files"_n, file > files;

  // Node table is scoped by file name, record indexed by node id.
  struct [[eosio::table]] node {
    uint64_t                id;
    vector<unsigned char>   data;
    uint64_t primary_key() const { return id; }
  };

  typedef eosio::multi_index< "nodes"_n, node > nodes;

  /*
    Create a new file.
   */
  [[eosio::action]]
  void create( name owner, name filename ) {
    require_auth( owner );
    files fls( _self, filename.value );
    auto pit = fls.begin();
    check( pit == fls.end(), "File exists." );
    fls.emplace( owner, [&]( auto& p ) {
      p.owner = owner;
      p.top = 0;
      p.published = false;
    });
  }

  /*
    Reset file data.
   */
  [[eosio::action]]
  void reset( name owner, name filename ) {
    files fls( _self, filename.value );
    files::const_iterator pit = auth_and_find_file( owner, filename, fls );
    fls.modify( pit, same_payer, [&]( auto& p ) {
      p.top = 0;
      p.published = false;
    });
    clear_nodes( filename );
  }

  /*
    Delete file.
  */
  [[eosio::action]]
  void del( name owner, name filename ) {
    files fls( _self, filename.value );
    files::const_iterator pit = auth_and_find_file( owner, filename, fls );
    fls.erase( pit );
    clear_nodes( filename );
  }

  /*
    Modify the file's published flag.
   */
  [[eosio::action]]
  void setpub( name owner, name filename, bool ispub ) {
    files fls( _self, filename.value );
    files::const_iterator pit = auth_and_find_file( owner, filename, fls );
    fls.modify( pit, same_payer, [&]( auto& p ) {
      p.published = ispub;
    });
  }

  /*
    Set file to immutable (set owner to invalid account name).
   */
  [[eosio::action]]
  void setimmutable( name owner, name filename ) {
    files fls( _self, filename.value );
    files::const_iterator pit = auth_and_find_file( owner, filename, fls );
    check( pit->published, "File not published." );
    fls.modify( pit, same_payer, [&]( auto& p ) {
      p.owner = ".immutable."_n;
    });
  }

  /*
    Assign data to a node of an existing file.
    A modified file is set to unpublished.
    Cannot assign empty data using setnode (use delnode or reset instead).
    Cannot assign non-empty data to any node above the top node.
  */
  [[eosio::action]]
  void setnode( name owner, name filename, uint64_t nodeid, vector<unsigned char> nodedata ) {
    check( nodedata.size() > 0, "Empty nodedata." );

    files fls( _self, filename.value );
    files::const_iterator pit = auth_and_find_file( owner, filename, fls );
    check( nodeid <= pit->top, "Past top." );
    fls.modify( pit, same_payer, [&]( auto& p ) {
      p.published = false;
      if ( p.top == nodeid )
	++p.top;
    });
    
    nodes nds( _self, filename.value );
    auto nit = nds.find( nodeid );
    if (nit == nds.end()) {
      nds.emplace( owner, [&]( auto& n ) {
	n.id = nodeid;
	n.data = nodedata;
      });
    } else {
      nds.modify( nit, same_payer, [&]( auto& n ) {
	n.data = nodedata;
      });
    }
  }

  /*
    Pops off (erases) the top data node of file.
   */
  [[eosio::action]]
  void delnode( name owner, name filename ) {
    files fls( _self, filename.value );
    files::const_iterator pit = auth_and_find_file( owner, filename, fls );
    uint64_t top = pit->top;
    check( top > 0, "Empty file." );
    --top;
    fls.modify( pit, same_payer, [&]( auto& p ) {
      p.top = top;
      p.published = false;
    });
    nodes nds( _self, filename.value );
    auto nit = nds.find( top );
    nds.erase( nit );
  }

private:

  void clear_nodes( name filename ) {
    nodes nds( _self, filename.value );
    auto nit = nds.begin();
    while ( nit != nds.end() ) {
      nds.erase(nit++);
    }
  }

  files::const_iterator auth_and_find_file( name owner, name filename, const files & fls ) {
    require_auth( owner );
    files::const_iterator pit = fls.begin();
    check( pit != fls.end(), "File does not exist." );
    check( pit->owner == owner, "Not file owner." );
    return pit;
  }

};
