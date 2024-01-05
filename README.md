# Personal Data Store

A basic implementation of a personal data store in C. This implements all the classic database operations: create, read, update, and delete.

Main functions are:  
2. `put_rec_by_key`: Store the given record at the END of the data file  
3. `get_rec_by_non_ndx_key`: Search by non-index key  
4. `get_rec_by_ndx_key`: Search by index key using offset in BST  
5. `delete_rec_by_ndx_key`: Delete by index key  

## Usage:
```
gcc -o pds_tester bst.c contact.c pds.c pds_tester.c
pds_tester testcase.in
```
