# PermaStore

PermaStore is a general-purpose multi-part binary storage standard for Antelope

For a sample web application that retrieves PermaStore files from the blockchain, check out [PermaServe](https://github.com/FluxBP/pserve).

# Downloading and uploading files

To download or upload files to PermaStore, one option is to use the standard `cleos` command-line tool that comes with Antelope implementations. Then it's just a matter of understanding what all the actions on the PermaStore contract do, which can be gleaned from the `pstore.cpp` source file. 

Another option is to use the `storeos` command-line tool. It is a single-file Perl script that can be downloaded from the root directory of the [PermaServe](https://github.com/FluxBP/pserve) repository. Run the script without arguments to see the tool help.

# Known deployments

* [UX Network](https://uxnetwork.io): account [permastoreux](https://explorer.uxnetwork.io/account/permastoreux)
