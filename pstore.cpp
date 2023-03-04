/*

  PermaStore

  A simple contract that allows the storage of arbitrary amounts of binary data
  into an Antelope blockchain's RAM.

  Users ("owner") can create "pages" (multipart binary files) that share the same
  global namespace of Antelope 64-bit names. Page names are first-come, first-serve.

  Once a page is created by its owner account, the owner can set data nodes (parts)
  on it, starting from node 0 and onwards (data nodes must be contiguous).

  Once the data upload is done, the page can be flagged as published (ready).
  Published pages can also be set to immutable.

  Notes:
  
  A good node size limit is 64,000 bytes, given that some Linux systems have 128kb
  command-line limits. You will need to post the entire data for a node on the
  command line as a hexadecimal text when using cleos, which will bloat it to
  128,000 bytes, leaving a good room of 3,072 bytes for the rest of the cleos
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

  // Page table is scoped by page name, record is a singleton.
  struct [[eosio::table]] page {
    name                    owner;      // account that controls the page (0 == no one / immutable)
    uint32_t                top;        // first empty node after last data node
    bool                    published;  // if the page is ready for use
    uint64_t primary_key() const { return 0; }
  };

  typedef eosio::multi_index< "pages"_n, page > pages;

  // Node table is scoped by page name, record indexed by node id.
  struct [[eosio::table]] node {
    uint64_t                id;
    vector<unsigned char>   data;
    uint64_t primary_key() const { return id; }
  };

  typedef eosio::multi_index< "nodes"_n, node > nodes;

  /*
    Create a new page.
   */
  [[eosio::action]]
  void create( name owner, name pagename ) {
    require_auth( owner );
    pages pgs( _self, pagename.value );
    auto pit = pgs.begin();
    check( pit == pgs.end(), "Page exists." );
    pgs.emplace( owner, [&]( auto& p ) {
      p.owner = owner;
      p.top = 0;
      p.published = false;
    });
  }

  /*
    Reset page data.
   */
  [[eosio::action]]
  void reset( name owner, name pagename ) {
    pages pgs( _self, pagename.value );
    pages::const_iterator pit = auth_and_find_page( owner, pagename, pgs );
    pgs.modify( pit, same_payer, [&]( auto& p ) {
      p.top = 0;
      p.published = false;
    });
    clear_nodes( pagename );
  }

  /*
    Delete page.
  */
  [[eosio::action]]
  void del( name owner, name pagename ) {
    pages pgs( _self, pagename.value );
    pages::const_iterator pit = auth_and_find_page( owner, pagename, pgs );
    pgs.erase( pit );
    clear_nodes( pagename );
  }

  /*
    Modify the page's published flag.
   */
  [[eosio::action]]
  void setpub( name owner, name pagename, bool ispub ) {
    pages pgs( _self, pagename.value );
    pages::const_iterator pit = auth_and_find_page( owner, pagename, pgs );
    pgs.modify( pit, same_payer, [&]( auto& p ) {
      p.published = ispub;
    });
  }

  /*
    Set page to immutable (set owner to invalid account name).
   */
  [[eosio::action]]
  void setimmutable( name owner, name pagename ) {
    pages pgs( _self, pagename.value );
    pages::const_iterator pit = auth_and_find_page( owner, pagename, pgs );
    check( pit->published, "Page not published." );
    pgs.modify( pit, same_payer, [&]( auto& p ) {
      p.owner = ".immutable."_n;
    });
  }

  /*
    Assign data to a node of an existing page.
    A modified page is set to unpublished.
    Cannot assign empty data using setnode (use delnode instead).
    Cannot assign non-empty data to any node above the top node.
  */
  [[eosio::action]]
  void setnode( name owner, name pagename, uint64_t nodeid, vector<unsigned char> nodedata ) {
    check( nodedata.size() > 0, "Empty nodedata." );

    pages pgs( _self, pagename.value );
    pages::const_iterator pit = auth_and_find_page( owner, pagename, pgs );
    check( nodeid <= pit->top, "Past top." );
    pgs.modify( pit, same_payer, [&]( auto& p ) {
      p.published = false;
      if ( p.top == nodeid )
	++p.top;
    });
    
    nodes nds( _self, pagename.value );
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
    Pops off (erases) the top data node of page.
   */
  [[eosio::action]]
  void delnode( name owner, name pagename ) {
    pages pgs( _self, pagename.value );
    pages::const_iterator pit = auth_and_find_page( owner, pagename, pgs );
    uint64_t top = pit->top;
    check( top > 0, "Empty page." );
    --top;
    pgs.modify( pit, same_payer, [&]( auto& p ) {
      p.top = top;
      p.published = false;
    });
    nodes nds( _self, pagename.value );
    auto nit = nds.find( top );
    nds.erase( nit );
  }

private:

  void clear_nodes( name pagename ) {
    nodes nds( _self, pagename.value );
    auto nit = nds.begin();
    while ( nit != nds.end() ) {
      nds.erase(nit++);
    }
  }

  pages::const_iterator auth_and_find_page( name owner, name pagename, const pages & pgs ) {
    require_auth( owner );
    pages::const_iterator pit = pgs.begin();
    check( pit != pgs.end(), "Page does not exist." );
    check( pit->owner == owner, "Not page owner." );
    return pit;
  }

};
