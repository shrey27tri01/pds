#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<string.h>

#include "pds.h"
#include "bst.h"

struct PDS_RepoInfo repo_handle;
   

int pds_create(char *repo_name) 
{
  char filename[30], indexfile[30];
  strcpy(filename,repo_name);
  strcpy(indexfile,repo_name);
  strcat(filename,".dat");
  strcat(indexfile,".ndx");
  FILE *fp = fopen(filename,"wb+");
  FILE *ifp = fopen(indexfile,"wb+");
  if(fp  == NULL || ifp == NULL) return PDS_FILE_ERROR;\
  fclose(fp);
  fclose(ifp);
  
  return PDS_SUCCESS;
}


int pds_open(char* repo_name, int rec_size) // Same as before
{
  char data_file[50];
  char index_file[50];

  if (repo_handle.repo_status == PDS_REPO_OPEN) {
    return PDS_REPO_ALREADY_OPEN;
  }

  strcpy(data_file, repo_name);
  strcat(data_file, ".dat");

  strcpy(index_file, repo_name);
  strcat(index_file, ".ndx");

  strcpy(repo_handle.pds_name, repo_name);

  // Open the data file and index file in rb+ mode
  FILE* fp_data = fopen(data_file, "rb+");
  FILE* fp_ndx = fopen(index_file, "rb+");

  if(fp_data == NULL) {
    perror(data_file);
  }

  if(fp_ndx == NULL) {
    perror(index_file);
  }

  // Update the fields of PDS_RepoInfo appropriately
  repo_handle.pds_data_fp = fp_data;
  repo_handle.pds_ndx_fp = fp_ndx;
  repo_handle.repo_status = PDS_REPO_OPEN;
  repo_handle.rec_size = rec_size;
  repo_handle.pds_bst = NULL;

  // Build BST and store in pds_bst by reading index entries from the index file
  pds_load_ndx();

  // Close only the index file
  fclose(fp_ndx);

  return PDS_SUCCESS;
}

int pds_load_ndx() // Same as before
{
  // Internal function used by pds_open to read index entries into BST
  fseek(repo_handle.pds_ndx_fp, 0, SEEK_SET);

	struct PDS_NdxInfo index_entry;
  repo_handle.pds_bst = NULL;
  // rewind(repo_handle.pds_ndx_fp);

  while(fread(&index_entry, sizeof(index_entry), 1, repo_handle.pds_ndx_fp) == 1) {

	  struct PDS_NdxInfo *ndx_row = (struct PDS_NdxInfo*)malloc(sizeof(struct PDS_NdxInfo));
	        
    ndx_row->key = index_entry.key;
	  ndx_row->offset = index_entry.offset;
	  
    int bst_add_status = bst_add_node(&repo_handle.pds_bst, index_entry.key, ndx_row);
	  
    if(bst_add_status != BST_SUCCESS) {
      return PDS_NDX_SAVE_FAILED;
    }
	}
  return PDS_SUCCESS;
}

int put_rec_by_key(int key, void*rec)
{
  // Seek to the end of the data file
  fseek(repo_handle.pds_data_fp, 0, SEEK_END);

  // Create an index entry with the current data file location using ftell
  struct PDS_NdxInfo * new_object = malloc(sizeof(struct PDS_NdxInfo));
  
  // ftell(repo_handle.pds_data_fp); 

  new_object->key = key;
  new_object->offset = ftell(repo_handle.pds_data_fp);

  // ENSURE is_deleted is set to 0 when creating index entry
  new_object->is_deleted = 0;

  // Add index entry to BST using offset returned by ftell
  int bst_add_status = bst_add_node(&repo_handle.pds_bst, key, new_object);

  // Write the key at the current data file location
  fwrite(&key, sizeof(int), 1, repo_handle.pds_data_fp);

  // Write the record after writing the key
  fwrite(rec, repo_handle.rec_size, 1, repo_handle.pds_data_fp);

  if (bst_add_status != BST_SUCCESS) {
    free(new_object);
    fseek(repo_handle.pds_data_fp, new_object->offset, SEEK_SET);
    return bst_add_status;
  } 

  return bst_add_status;


}

int get_rec_by_ndx_key(int key, void*rec)
{
  // Search for index entry in BST
  struct BST_Node *node;
  node = bst_search(repo_handle.pds_bst, key);
  
  if (node == NULL) {
    return PDS_REC_NOT_FOUND;
  }

  // Check if the entry is deleted and if it is deleted, return PDS_REC_NOT_FOUND
  struct PDS_NdxInfo *ndx_entry = node->data;

  if (ndx_entry->is_deleted == 1) {
    return PDS_REC_NOT_FOUND;
  }

  // Seek to the file location based on offset in index entry
  fseek(repo_handle.pds_data_fp, ndx_entry->offset, SEEK_SET);

  // Read the key at the current file location 
  fread(&key, sizeof(key), 1, repo_handle.pds_data_fp);

  // Read the record after reading the key
  fread(rec, repo_handle.rec_size, 1, repo_handle.pds_data_fp);

  return PDS_SUCCESS;
}

int preorder(struct BST_Node* root, FILE * fptr)
{
  if (root == NULL){
    return PDS_SUCCESS;
  }

  struct PDS_NdxInfo *ndx = (struct PDS_NdxInfo *)root->data;

	int status = 1;

  // Ignore the index entries that have already been deleted.
	if(ndx->is_deleted == 0) {
		status = fwrite(ndx, sizeof(struct PDS_NdxInfo), 1, fptr);
	}

  if(status == 0) {
    return PDS_NDX_SAVE_FAILED;
  }
 
  int po_left = preorder(root->left_child, fptr); 
  int po_right = preorder(root->right_child, fptr);

  if(po_left != PDS_SUCCESS || po_right != PDS_SUCCESS) {
    return PDS_NDX_SAVE_FAILED;
  }
  
  return PDS_SUCCESS;
}

int pds_close() 
{
  // Open the index file in wb mode (write mode, not append mode)
  FILE* ndxFile = fopen(strcat(repo_handle.pds_name, ".ndx"), "wb");

  if (ndxFile == NULL) {
    return PDS_FILE_ERROR;
  }

  // Unload the BST into the index file by traversing it in PRE-ORDER (overwrite the entire index file)
  int status = preorder(repo_handle.pds_bst, ndxFile);

  if (status == PDS_NDX_SAVE_FAILED) {
    return PDS_NDX_SAVE_FAILED;
  }

  // Ignore the index entries that have already been deleted.
  // Done in preorder()

  // Free the BST by calling bst_destroy()
  bst_destroy(repo_handle.pds_bst);

  repo_handle.pds_bst = NULL;
  repo_handle.repo_status = PDS_REPO_CLOSED;
  strcpy(repo_handle.pds_name,"");

  // Close the index file and data file
  fclose(ndxFile);
  fclose(repo_handle.pds_data_fp);

  return PDS_SUCCESS;

}

int get_rec_by_non_ndx_key(void *key, void *rec, int (*matcher)(void *rec, void *key), int *io_count)
{
  // Seek to beginning of file
  fseek(repo_handle.pds_data_fp, 0, SEEK_SET);
  
  int temp_key;

  int next_key = fread(&temp_key, sizeof(int), 1, repo_handle.pds_data_fp);
  int next_rec = fread(rec, repo_handle.rec_size, 1, repo_handle.pds_data_fp);

  // Perform a table scan - iterate over all the records
  while(next_key == 1 && next_rec == 1) {
    //   Increment io_count by 1 to reflect count no. of records read
    (*io_count)++;

    //   Use the function in function pointer to compare the record with required key
    int status = matcher(rec, key);
    
	  if(status == 0) {

      // Check the entry of the record in the BST and see if it is deleted. If so, return PDS_REC_NOT_FOUND
      struct BST_Node *node = bst_search(repo_handle.pds_bst, temp_key);

      if(node == NULL) {
        return PDS_REC_NOT_FOUND;
      }

      struct PDS_NdxInfo * tmp_ndx = (struct PDS_NdxInfo *)node->data;

      if(tmp_ndx->is_deleted == 1) {
        return PDS_REC_NOT_FOUND;
      }

      // Return success when record is found
      return PDS_SUCCESS;
	
	  }
    //   Read the key and the record
    next_key = fread(&temp_key, sizeof(int), 1, repo_handle.pds_data_fp);
    next_rec = fread(rec, repo_handle.rec_size, 1, repo_handle.pds_data_fp);
  }

  return PDS_REC_NOT_FOUND;
}

int delete_rec_by_ndx_key( int key) // New Function
{
  // Search for the record in the BST using the key
  struct BST_Node *node;
  node = bst_search(repo_handle.pds_bst, key);

  // If record not found, return PDS_DELETE_FAILED
  if (node == NULL) {
    return PDS_DELETE_FAILED;
  }

  // If record is found, check if it has already been deleted, if so return PDS_DELETE_FAILED
  struct PDS_NdxInfo *ndx_entry = node->data;

  if (ndx_entry->is_deleted == 1) {
    return PDS_DELETE_FAILED;
  }

  // Else, set the record to deleted and return PDS_SUCCESS
  ndx_entry->is_deleted = 1;
  return PDS_SUCCESS;

  
}
