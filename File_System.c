// Marisa Dominguez 1001761302
// Naila Imran 1001853558

// The MIT License (MIT)
// 
// Copyright (c) 2021, 2022 Trevor Bakker 
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES UTA OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>

#define NUM_BLOCKS 4226
#define BLOCK_SIZE 8192
#define NUM_FILES 128
#define NUM_INODES 128
#define MAX_BLOCKS_PER_FILE 32
#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                                // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255    // The maximum command-line size

#define MAX_NUM_ARGUMENTS 5     // Mav shell only supports five arguments




static FILE* ifp;

unsigned char data_blocks[NUM_BLOCKS][BLOCK_SIZE];
unsigned char * used_blocks;


struct iNode
{
   time_t date;
   int valid;
   int size;
   int blocks[MAX_BLOCKS_PER_FILE];
   int hidden;
   int read;
};
struct iNode * iNodePtr;

struct iNodeMap
{
   int inUse;
};

struct BlockMap
{
   int inUse;
};

struct Directory
{
   char name[32];
   int valid;
   int iNodeIndex;
};
struct Directory * directoryPtr;


void init ()
{
   int i;

   // setting the data_blocks values to 0
   memset(&data_blocks[0], 0, 4226*8192);
   
   // pointing the directory pointer to data_block 0
   directoryPtr = (struct Directory*) &data_blocks[0];

   for(i=0; i<NUM_FILES; i++)
   {
      directoryPtr[i].valid = 0;
   }

   int iNodeIndex = 0;

   // pointing the iNode pointer to start from data_blocks index 5
   // since we are using blocks 5 - 130 for the filesystem image
   iNodePtr = (struct iNode*) &data_blocks[5];

   for(i=0; i<125; i++)
   {
      // for loop from 0 to 32
      // set all the blocks to -1
      int j;
      for( j = 0; j< 32; j++ )
      {
         iNodePtr[i].blocks[j] = -1;
      }
	   
      iNodePtr[i].hidden = 0;
      iNodePtr[i].read = 0;
   } 
  
  // we are using data_block 3 for our free block map
   used_blocks = (unsigned char*)&data_blocks[3];
   
}

// function returns us a valid directory entry
// we may have enough bytes free but we have to make sure
// we have enough space in our directory
// since we can only store up 125 files
int findFreeDirectoryEntry ()
{
   int i;
   int retval = -1;
   for(i=0; i<128; i++)
   {
      if(directoryPtr[i].valid == 0)
      {
         retval = i;
         break;
      }
   }
   return retval;
}

// function goes through iNode array 
// and finds a valid entry
int findFreeInode ()
{
   int i;
   int retval = -1;
   for(i=0; i<128; i++)
   {
      if(iNodePtr[i].valid == 0)
      {
         retval = i;
         break;
      }
   }
   
   return retval;
}

int findFreeInodeBlockEntry(int iNodeIndex)
{
   int i;
   int retval = -1;
   for(i=0; i<32; i++)
   {
      if(iNodePtr[iNodeIndex].blocks[i] == -1)
      {
         retval = i;
         break;
      }
   }
   return retval;
}

// finding a free block to store file's information
int findFreeBlock()
{
   int retval = -1;
   int i = 0;

   for(i=130; i<4226; i++)
   {
      if(used_blocks[i]==0)
      {
         retval = i;
         break;
      }
   }
   return retval;
}

// function that is called when user inputs "df"
// list the amount of bytes available 
// we will also use this function when doing a put();

int df ()
{
   int count = 0;
   int i = 0;
   // iterating over the used block array and if it's == 0
   // then it means it's unused
   for(i=130; i<4226; i++)
   {
      if(used_blocks[i]==0)
      {
         count++;
      }
   }
   return count * BLOCK_SIZE;
}



void get(char * filename, char * newFilename)
{
   int i;
   if(newFilename == NULL)
   {
      for(i = 0; i<NUM_FILES; i++)
      {
         // check if file exists in the directory
         if(strcmp(directoryPtr[i].name, filename)==0)
         {
            int length = strlen(filename);

            if(length > 32)
            {
               printf("put error: File name too long.");
               return;
            }

 
            FILE *ifp;
            // if user does not input a new file name
            // get the file name input
            if( newFilename == NULL )
            {
            ifp = fopen (filename, "w"); 
            }
            else
            {
            // if user does input a new file name
            // open the new file
               ifp = fopen( newFilename, "w");
            }


 
            // Save off the size of the input file since we'll use it in a couple of places and 
            // also initialize our index variables to zero. 
            int copy_size = iNodePtr[directoryPtr[i].iNodeIndex].size;
            int inode_index = directoryPtr[i].iNodeIndex;
            // We want to copy and write in chunks of BLOCK_SIZE. So to do this 
            // we are going to use fseek to move along our file stream in chunks of BLOCK_SIZE.
            // We will copy bytes, increment our file pointer by BLOCK_SIZE and repeat.
            int offset = 0;
            int block_index =  0;
            // copy_size is initialized to the size of the input file so each loop iteration we
            // will copy BLOCK_SIZE bytes from the file then reduce our copy_size counter by
            // BLOCK_SIZE number of bytes. When copy_size is less than or equal to zero we know
            // we have copied all the data from the input file.
            while(copy_size >= BLOCK_SIZE)
            {
      
               // We are going to copy and store our file in BLOCK_SIZE chunks instead of one big 
               // memory pool. Why? We are simulating the way the file system stores file data in
               // blocks of space on the disk. block_index will keep us pointing to the area of
               // the area that we will read from or write to.


               // Read BLOCK_SIZE number of bytes from the input file and store them in our
               // data array. 
               int bytes = fwrite(&data_blocks[iNodePtr[inode_index].blocks[block_index]], BLOCK_SIZE, 1, ifp);

               // If bytes == 0 and we haven't reached the end of the file then something is 
               // wrong. If 0 is returned and we also have the EOF flag set then that is OK.
               // It means we've reached the end of our input file.
               if(bytes == 0 )
               {
                  printf("An error occured writing to the output file.\n");
                  return;
               }

               // Reduce copy_size by the BLOCK_SIZE bytes.
               copy_size -= BLOCK_SIZE;
      
               // Increase the offset into our input file by BLOCK_SIZE.  This will allow
               // the fseek at the top of the loop to position us to the correct spot.
               offset += BLOCK_SIZE;

               // Increment the index into the block array 
               block_index ++;
            }
            // handle remainder 
            if(copy_size > 0)
            {
               fwrite(&data_blocks[iNodePtr[inode_index].blocks[block_index]], copy_size, 1, ifp);

            }
            fclose(ifp);
            return;
         }
      }
   }

   else
   {
      for(i = 0; i<NUM_FILES; i++)
      {
         if(strcmp(directoryPtr[i].name, filename)==0)
         {
            int length = strlen(filename);

            if(length > 32)
            {
               printf("put error: File name too long.");
               return;
            }

            FILE *ifp;
            // if user does not input a new file name
            // get the file name input
            if( newFilename == NULL )
            {
            ifp = fopen (filename, "w"); 
            }
            else
            {
            // if user does input a new file name
            // open the new file
               ifp = fopen( newFilename, "w");
            }




            
            // Save off the size of the input file since we'll use it in a couple of places and 
            // also initialize our index variables to zero. 
            int copy_size = iNodePtr[directoryPtr[i].iNodeIndex].size;
            int inode_index = directoryPtr[i].iNodeIndex;
            // We want to copy and write in chunks of BLOCK_SIZE. So to do this 
            // we are going to use fseek to move along our file stream in chunks of BLOCK_SIZE.
            // We will copy bytes, increment our file pointer by BLOCK_SIZE and repeat.
            int offset = 0;
            int block_index =  0;
            // copy_size is initialized to the size of the input file so each loop iteration we
            // will copy BLOCK_SIZE bytes from the file then reduce our copy_size counter by
            // BLOCK_SIZE number of bytes. When copy_size is less than or equal to zero we know
            // we have copied all the data from the input file.
            while(copy_size >= BLOCK_SIZE)
            {
      
               // We are going to copy and store our file in BLOCK_SIZE chunks instead of one big 
               // memory pool. Why? We are simulating the way the file system stores file data in
               // blocks of space on the disk. block_index will keep us pointing to the area of
               // the area that we will read from or write to.


               // Read BLOCK_SIZE number of bytes from the input file and store them in our
               // data array. 
               int bytes = fwrite(&data_blocks[iNodePtr[inode_index].blocks[block_index]], BLOCK_SIZE, 1, ifp);

               // If bytes == 0 and we haven't reached the end of the file then something is 
               // wrong. If 0 is returned and we also have the EOF flag set then that is OK.
               // It means we've reached the end of our input file.
               if(bytes == 0 )
               {
                  printf("An error occured writing to the output file.\n");
                  return;
               }

               // Reduce copy_size by the BLOCK_SIZE bytes.
               copy_size -= BLOCK_SIZE;
      
               // Increase the offset into our input file by BLOCK_SIZE.  This will allow
               // the fseek at the top of the loop to position us to the correct spot.
               offset += BLOCK_SIZE;

               // Increment the index into the block array 
               block_index ++;
            }
            // handle remainder
            if(copy_size > 0)
            {
               fwrite(&data_blocks[iNodePtr[inode_index].blocks[block_index]], copy_size, 1, ifp);

            }
            fclose(ifp);
            return;
         }
      }
   }
}

void list()
{
   char * fileName;
   int fileSize = 0;
   int holdIndex = 0;
   char fileTime[50];

   for(int i=0; i<NUM_FILES; i++)
   {
      //if directory is in use
      if (directoryPtr[i].valid == 1)
      {
         // save file name into variable
         fileName = directoryPtr[i].name;
         // hold index of iNode pointer
         holdIndex = directoryPtr[i].iNodeIndex;
         // if a certian file iNodePtr is hidden we don't list that file out
         if(iNodePtr[holdIndex].hidden == 1)
         {
            continue;
         }
         // store file size in var
         fileSize = iNodePtr[holdIndex].size;
         //copy the time string into file time variable
         // without the extra end of line character
         strncpy(fileTime, ctime(&iNodePtr[holdIndex].date), (strlen(ctime(&iNodePtr[holdIndex].date))-1));
         printf("%d   %s   %s\n",fileSize,fileTime,fileName);
      }
   }
   
}


int attrib(char * filename, char * fileAttribute)
{
   int i=0;
   int holdIndex=0;
   int foundFile=0;
   int comp1 = strcmp(fileAttribute, "+h");

   // if "+h" tag hide file name and do not list out when listing all files
   if(comp1 == 0)
   {
      for(i=0; i<NUM_FILES; i++)
      {
         if(strcmp(directoryPtr[i].name, filename)==0)
         {
            holdIndex = directoryPtr[i].iNodeIndex;
            foundFile = 1;
            break;
         }
      }
      
      if(foundFile == 0)
      {
         return -1;
      }
      
      else
      {
         iNodePtr[holdIndex].hidden = 1;
         return 0;  
      }
   }

   // if "-h" remove hidden tag and show file when listing files out 
   int comp2 = strcmp(fileAttribute, "-h");
   if(comp2 == 0)
   {
      for(i=0; i<NUM_FILES; i++)
      {
         if(strcmp(directoryPtr[i].name, filename)==0)
         {
            holdIndex = directoryPtr[i].iNodeIndex;
            foundFile = 1;
            break;
         }
      }
      if(foundFile == 0)
      {
         return -1;
      }
      else
      {
         iNodePtr[holdIndex].hidden = 0;
         return 0;
      }
   }

   // if "+r" add read tag so file cannot be deleted
   int comp3 = strcmp(fileAttribute,"+r");
   if(comp3 == 0)
   {
      for(i=0; i<NUM_FILES; i++)
      {
         if(strcmp(directoryPtr[i].name, filename)==0)
         {
            holdIndex = directoryPtr[i].iNodeIndex;
            foundFile = 1;
            break;
         }
      }
      
      if(foundFile == 0)
      {
         return -1;
      }
      else
      {
         iNodePtr[holdIndex].read = 1;
         return 0;
      }

   }

   // if "-r" remove read tag to make it possible to delete file
   int comp4 = strcmp(fileAttribute,"-r");
   if(comp4 == 0)
   {
      for(i=0; i<NUM_FILES; i++)
      {
         if(strcmp(directoryPtr[i].name, filename)==0)
         {
            holdIndex = directoryPtr[i].iNodeIndex;
            foundFile = 1;
            break;
         }
      }
      if(foundFile == 0)
      {
         return -1;
      }
      else
      {
         iNodePtr[holdIndex].read = 0;
         return 0;
      }

   }
   return 0;
}


int del(char * filename)
{
   int holdIndex = 0;
   int foundFile = 0;
   int fileHasReadAttrib = 0;
   int i;
   for(i=0; i<NUM_FILES; i++)
   {
      // if we find the file
      if(strcmp(directoryPtr[i].name, filename)==0)
      {
         // find the index and store it in the holdIndex var
         holdIndex = directoryPtr[i].iNodeIndex;
         // read is set to 1 we cannot delete the file
         if(iNodePtr[holdIndex].read == 1)
         {
            
            fileHasReadAttrib = 1;
            break;
         }
         // set the directory index file back to 0
         // making the index valid to use again
         directoryPtr[i].valid = 0;
         
         foundFile = 1;
         break;
      }
   }
   
   // if we don't find a file or read == 1 
   // nothing can be deleted
   if(foundFile == 0 && fileHasReadAttrib == 0)
   {
      return -1;
   }
   else if(foundFile == 0 && fileHasReadAttrib == 1)
   {
      return -2;
   }
   else
   {
      // set valid == 0 so that the iNode index can be used again
      iNodePtr[holdIndex].valid = 0;
      int j;
      // loop through the blocks the file is using to store info
      // marking them to not in use
      for(j = 0; j< 32; j++)
      {
         if( iNodePtr[holdIndex].blocks[j]!= -1 )
         {
            used_blocks[iNodePtr[holdIndex].blocks[j]] = 0;
         }
      }

      return 0;
   }

}


int undel(char * filename)
{
   int holdIndex = 0;
   int foundFile = 0;
   int i;
   int filesize = 0;
   for(i=0; i<NUM_FILES; i++)
   {
      // find the file name
      if(strcmp(directoryPtr[i].name, filename)==0)
      {
         // set the directory index back to 1 for the file
         directoryPtr[i].valid = 1;
         holdIndex = directoryPtr[i].iNodeIndex;
         foundFile = 1;
         filesize = iNodePtr[holdIndex].size;
         //save the file size
         break;
      }
   }

   // checks if we have enough space to add the fie again
   if( filesize >  df () )
   {
      return -2;
   }
   // if we can't find file return
   if(foundFile == 0)
   {
      return -1;
   }
   else
   {

      // set the iNodePtr index back to in use
      iNodePtr[holdIndex].valid = 1;
      int j;
      for( j = 0; j< 32; j++ )
      {
         // restore the file information into the blocks
         used_blocks[iNodePtr[holdIndex].blocks[j]] = 1;
      }
      return 0;
   }
  
}

void put (char * filename)
{
   // get file length
   int length = strlen(filename);

   // make sure file name is correct length
   if(length > 32)
   {
      printf("put error: File name too long.");
      return;
   }
   
   // will check if file exists
   // will store the file's meta data into the stat struct
   struct stat buf;

   // pass in filename and buffer struct to stat ()
   int status = stat(filename, &buf);

   // if file does not exist stat will return -1 
   if(status == -1)
   {
      printf("Error: File not found.\n");
      return;
   }

   // will check if we have enough space in our filesystem
   // because df() checks for how many bytes we have left to use
   if(buf.st_size > df())
   {
      printf("put error: Not enough disk space.\n");
      return;
   }
   
   // check if the file size is too big
   if(buf.st_size > 10240000)
   {
      printf("put error: File is too big.\n");
      return;
   }
   
   // check if we a directory entry to add the new file to
   int dir_idx = findFreeDirectoryEntry();
   
   // if not then return "not enough disk space"
   if(dir_idx == -1)
   {
      printf("put error: Not enough disk space.\n");
      return;
   }

   // valid = 1 here means we're using the directory index
   // so next time we add a file
   // this index will not be overwritten
   directoryPtr[dir_idx].valid = 1;
   
   // adding in the directory struct information
   
   // copy the inputted file name into directory struct name variable
   strncpy(directoryPtr[dir_idx].name, filename, strlen(filename));

   // now we have to find a free iNode index
   int inode_idx = findFreeInode();

   // if returning -1 we don't have enough inodes for file information
   if(inode_idx == -1)
   {
      printf("Error: No free inodes.\n");
      return;
   }

   // storing the found iNode index in the directory
   directoryPtr[dir_idx].iNodeIndex = inode_idx;
   // storing the size into iNode struct
   iNodePtr[inode_idx].size = buf.st_size;
   // storing the date the file was added into iNode struct
   iNodePtr[inode_idx].date = time(NULL);
   // updating the iNode to currently in use
   iNodePtr[inode_idx].valid = 1;
   
   // Open the input file read-only
   FILE *ifp = fopen ( filename, "r" ); 
 
   // Save off the size of the input file since we'll use it in a couple of places and 
   // also initialize our index variables to zero. 
   int copy_size = buf.st_size;

   // We want to copy and write in chunks of BLOCK_SIZE. So to do this 
   // we are going to use fseek to move along our file stream in chunks of BLOCK_SIZE.
   // We will copy bytes, increment our file pointer by BLOCK_SIZE and repeat.
   int offset = 0;

   // copy_size is initialized to the size of the input file so each loop iteration we
   // will copy BLOCK_SIZE bytes from the file then reduce our copy_size counter by
   // BLOCK_SIZE number of bytes. When copy_size is less than or equal to zero we know
   // we have copied all the data from the input file.
   while(copy_size >= BLOCK_SIZE)
   {
      
      // We are going to copy and store our file in BLOCK_SIZE chunks instead of one big 
      // memory pool. Why? We are simulating the way the file system stores file data in
      // blocks of space on the disk. block_index will keep us pointing to the area of
      // the area that we will read from or write to.
      int block_index = findFreeBlock();
    
      if(block_index == -1)
      {
         printf("Error: Can't find free block.\n");
         return;
      }

      used_blocks[block_index] = 1;

      int inodeBlockEntry = findFreeInodeBlockEntry(inode_idx);
      if (inodeBlockEntry == -1)
      {
         printf("Error: Can't find free node block.\n");
         return;
      }
      iNodePtr[inode_idx].blocks[inodeBlockEntry] = block_index;

      // Index into the input file by offset number of bytes.  Initially offset is set to
      // zero so we copy BLOCK_SIZE number of bytes from the front of the file.  We 
      // then increase the offset by BLOCK_SIZE and continue the process.  This will
      // make us copy from offsets 0, BLOCK_SIZE, 2*BLOCK_SIZE, 3*BLOCK_SIZE, etc.
      fseek(ifp, offset, SEEK_SET);

      // Read BLOCK_SIZE number of bytes from the input file and store them in our
      // data array. 
      int bytes = fread(data_blocks[block_index], BLOCK_SIZE, 1, ifp);

      // If bytes == 0 and we haven't reached the end of the file then something is 
      // wrong. If 0 is returned and we also have the EOF flag set then that is OK.
      // It means we've reached the end of our input file.
      if(bytes == 0 && !feof(ifp))
      {
         printf("An error occured reading from the input file.\n");
         return;
      }

      // Clear the EOF file flag.
      clearerr(ifp);

      // Reduce copy_size by the BLOCK_SIZE bytes.
      copy_size -= BLOCK_SIZE;
      
      // Increase the offset into our input file by BLOCK_SIZE.  This will allow
      // the fseek at the top of the loop to position us to the correct spot.
      offset += BLOCK_SIZE;

      // Increment the index into the block array 
      block_index ++;
   }

   if(copy_size > 0)
   {
      // We are going to copy and store our file in BLOCK_SIZE chunks instead of one big 
      // memory pool. Why? We are simulating the way the file system stores file data in
      // blocks of space on the disk. block_index will keep us pointing to the area of
      // the area that we will read from or write to.
      int block_index = findFreeBlock();
    
      if(block_index == -1)
      {
         printf("Error: Can't find free block.\n");
         return;
      }

      int inodeBlockEntry = findFreeInodeBlockEntry(inode_idx);
      if (inodeBlockEntry == -1)
      {
         printf("Error: Can't find free node block.\n");
         return;
      }
      iNodePtr[inode_idx].blocks[inodeBlockEntry] = block_index;

      used_blocks[block_index] = 1;

      //Handle remainder
      fseek(ifp, offset, SEEK_SET);
      int bytes = fread(data_blocks[block_index], copy_size, 1, ifp);
   }
   fclose(ifp);
   return;
}


int main()
{

   char * cmd_str = (char*) malloc(MAX_COMMAND_SIZE);

   while(1)
   {
      // Print out the mfs prompt
      printf ("mfs> ");

      // Read the command from the commandline.  The
      // maximum command that will be read is MAX_COMMAND_SIZE
      // This while command will wait here until the user
      // inputs something since fgets returns NULL when there
      // is no input
      while(!fgets (cmd_str, MAX_COMMAND_SIZE, stdin));

      // If the first char of the string we get from the command line is either
      // a new line character, a space, or a tab, then the user has typed a blank line
      // We continue to the top of the loop to print another prompt
      // and accept a new line of input 
      if (cmd_str[0]=='\n' || cmd_str[0]==' ' || cmd_str[0]=='\t')
      {
         continue;
      }

      /* Parse input */
      char *token[MAX_NUM_ARGUMENTS];

      int token_count = 0;                                 
                                                           
      // Pointer to point to the token
      // parsed by strsep
      char *arg_ptr;                                         
                                                           
      char *working_str  = strdup(cmd_str);                

      // we are going to move the working_str pointer so
      // keep track of its original value so we can deallocate
      // the correct amount at the end
      char *working_root = working_str;

      // Tokenize the input stringswith whitespace used as the delimiter
      while (((arg_ptr = strsep(&working_str, WHITESPACE)) != NULL) && 
              (token_count<MAX_NUM_ARGUMENTS))
      {
         token[token_count] = strndup(arg_ptr, MAX_COMMAND_SIZE);
         if(strlen(token[token_count]) == 0)
         {
            token[token_count] = NULL;
         }
         token_count++;
      }


      //Quit the application
      if(strcmp(token[0],"quit") == 0)
      {
         exit(0);
      }

      else if (strcmp(token[0],"df") == 0)
      {
         int dfRet = df();
         printf("%d bytes free.\n", dfRet); 
      }

      else if(strcmp(token[0],"list") == 0)
      {
         list();
      }

      else if(strcmp(token[0],"put") == 0)
      {
         put(token[1]);
      }

      else if(strcmp(token[0],"get") == 0)
      {
         get(token[1], token[2]);
      }
      
      else if(strcmp(token[0],"del") == 0)
      {
         int retVal = del(token[1]);
         if(retVal == -1)
         {
            printf("del error: File not found.\n");
         }

         if(retVal == -2)
         {
            printf("File can't be deleted.\n");
         }
      }

      else if(strcmp(token[0],"undel") == 0)
      {
         int retVal = undel(token[1]);
	      if(retVal == -1)
         {
            printf("undel error: File not found.\n");
         }
         if(retVal == -2){
            printf("undel error: Not enough disk space\n");
         }
      }

      else if(strcmp(token[0],"createfs") == 0)
      {
         if(token[1]==NULL)
         {
            printf("createfs: File not found.\n");
            continue;
         }

         init();
         ifp = fopen(token[1], "w");
         fwrite(&data_blocks[0], 8192, 4226, ifp);
         fclose(ifp);
      }

      else if(strcmp(token[0],"open") == 0)
      {
         init();
         ifp = fopen(token[1], "rb");
         if (ifp == NULL)
         {
            printf("Error: Cannot open file\n");
         }
         fread(&data_blocks[0], 8192, 4226, ifp);
         fclose( ifp );
      }

      else if(strcmp(token[0],"savefs") == 0)
      {
         ifp = fopen( token[1], "wb");
         fwrite(&data_blocks[0], 8192, 4226, ifp);
         fclose(ifp);
      }

      else if(strcmp(token[0],"close") == 0)
      {

      }

      else if(strcmp(token[0],"attrib") == 0)
      {
         int retVal = attrib(token[2], token[1]);
         if(retVal == -1)
         {
            printf("attrib: File not found.\n");
         }
      }


      free(working_root);

   }
   return 0;
}
