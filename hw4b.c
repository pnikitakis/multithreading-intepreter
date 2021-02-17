#include <unistd.h>     /* Symbolic Constants */
#include <sys/types.h>  /* Primitive System Data Types */ 
#include <errno.h>      /* Errors */
#include <stdio.h>      /* Input/Output */
#include <stdlib.h>     /* General Utilities */
#include <pthread.h>    /* POSIX Threads */
#include <string.h>
#include <time.h>
#include <math.h>
#include <ctype.h>
#include "khash.h"

#define FALSE 0
#define TRUE 1
#define MAX_LINES 100
#define MAX_LINE_LENGTH 100
#define MAX_WORDS_PER_LINE 10
#define MAX_WORD_LENGTH 30
#define MAX_PRINT_LENGTH 30
#define MAX_THREADS 1
#define UNINITIALIZED -23562
#define STOPPED 0
#define RUNNING 1
#define FINISHED 2
#define SLEEP 3
#define NUM_INSTR_CHANGE 5


#define CHECK_IF_VAR(token) \
	if(check_if_var( token ) == 0)  \
		return 1;

#define CHECK_IF_GLOBAL_VAR(token) \
	if (check_if_global_var( token ) == 1) \
        return 1;

#define CHECK_IF_VARVAL(token) \
	if (check_if_var( token ) == 0) \
		if ( checkIfInteger( token ) == FALSE ) \
            return 1; 
        
// shorthand way to get the key from hashtable or defVal if not found
#define kh_get_val(kname, hash, key, defVal) ({k=kh_get(kname, hash, key);(k!=kh_end(hash)?kh_val(hash,k):defVal);})
        
// shorthand way to set value in hash with single line command.  Returns value
// returns 0=replaced existing item, 1=bucket empty (new key), 2-adding element previously deleted
#define kh_set(kname, hash, key, val) ({int ret; k = kh_put(kname, hash,key,&ret); kh_value(hash,k) = val; ret;})

const int khStrInt = 33;
KHASH_MAP_INIT_STR(khStrInt, int); // setup khash to handle string key with int payload
khash_t(khStrInt) *h_glob; //for global Variables

const char s[] = " \t\r\n\v\f"; //delimeter lines by whitespace
const char p[] = "\"";  

struct plist{
    int id;
    char progName[MAX_WORD_LENGTH];
    int argc;
    char argv[MAX_WORDS_PER_LINE][MAX_WORD_LENGTH];
    char token[MAX_LINES][MAX_WORDS_PER_LINE][MAX_WORD_LENGTH];
    int compiled;
    int state;
    int pc;
    clock_t startSleepTime;
    int secOfSleep;
    khash_t(khStrInt) *h_loc;
    struct plist *nxt;
    struct plist *prv;
};

struct plist *head = NULL;


/**** LIST FUNCTIONS ********/

void list_init(){
	head = (struct plist *)malloc(sizeof(struct plist));
    head->nxt = head;
    head->prv = head;
}

//is list empty
int isEmpty() {
   return head == NULL;
}

void list_display() {
   //start from the beginning
   struct plist *ptr = head->nxt;
   printf("\n");
   while(ptr != head && ptr!= NULL) {        
      printf("[Id:%d | name: %s | state: %d | pc: %d]\n",ptr->id,ptr->progName, ptr->state, ptr->pc);
      ptr = ptr->nxt;
   }
	
   printf("\n");
}

void list_insert(int id, int argc, char argv[MAX_WORDS_PER_LINE][MAX_WORD_LENGTH]) {
    int i;
    
    struct plist *curr;
    curr = (struct plist *)malloc(sizeof(struct plist));
    
    //values
    curr->id = id;
    curr->state = STOPPED;
    curr->compiled = FALSE;
    curr->pc = 1;
    curr->argc = argc;
    
    memset(curr->progName, '\0', sizeof(curr->progName));
    strcpy(curr->progName, argv[1]);
    
    for(i=0; i < MAX_LINES; i++)
        memset(curr->token[i], '\0', sizeof(curr->token[i]));
    
    for(i=0; i < MAX_WORDS_PER_LINE; i++)
        memset(curr->argv[i], '\0', sizeof(curr->argv[i]));
    
    for(i=0; argv[i][0] != '\0'; i++){
        strcpy(curr->argv[i], argv[i]);
        printf("argv[%d]: %s\n", i, argv[i]);
        
    }
     
    //insert at head
    curr->nxt = head->nxt;
    curr->prv = head;
    curr->nxt->prv = curr; 
    curr->prv->nxt = curr;
}

int list_remove(int id){
    struct plist *curr;
    head->id = id;
    for(curr=head->nxt; curr->id!=id; curr=curr->nxt){}
    if (curr != head) {
        curr->nxt->prv = curr->prv;
        curr->prv->nxt = curr->nxt;
        free(curr);
        return(1);
    }
    return(0);
}



/**** GENERAL FUNCTIONS **************/

int checkIfInteger(char *str){
         
    int valid = TRUE;
    int i = 0;
    
    if (str[0] == '-') {
	i++;
    }	

    for ( ; i < strlen(str); i++)
    {
        if (!isdigit(str[i]))
        {
            valid = FALSE;
            break;
        }
    }
    return(valid);
}

int countLines(char* file_arg){ //inline int checkSyntax
    
    char buf[200];
    int count=0;
    
    FILE* f = fopen(file_arg, "r");
    if(f == NULL) {
      return(-1);
    }
    while(fgets(buf,sizeof(buf), f) != NULL){
        count++;
    }
    
    fclose(f);
    
    return(count);
}

int getTempTokenArray(char token[MAX_WORD_LENGTH], khash_t(khStrInt) *h){
    
    int i,j, k, value;
    char arg[MAX_WORD_LENGTH],
         value_str[MAX_WORD_LENGTH];
    khiter_t k_it;
    
    memset(value_str, '\0', sizeof(value_str));
    k=0;
    for (i=0 ; i < strlen(token); i++)
    {
        if (token[i] == '['){
            j=i+1;
            while(token[j] != ']'){
                arg[k] = token[j];
                j++;
                k++;
            }
            arg[k] = '\0';
            break;
        }
    }
    
    k_it = kh_get(khStrInt, h, arg);
    value = kh_val(h, k_it);
    if(value == UNINITIALIZED){
        return(-1);
    }
    
    sprintf(value_str, "%d", value);

    //eisagwgi sth leksh
    k=0;
    for (i=0 ; i < strlen(token); i++){
        if (token[i] == '['){
            for(j=0; j < strlen(value_str); ++j){
                token[i+1+j] = value_str[j];
            }
            token[i+1+j] = ']';
            token[i+1+j+1] = '\0';
        }
    }
    
    return 0;
}

//return 0:(val or array with integer arg), 1:(array with string arg)
int check_if_array(const char *token, khash_t(khStrInt) *h)  {

    int i,j, k, 
        isArray = FALSE;
    char arg[MAX_WORD_LENGTH];
    
    k=0;
    for (i=0 ; i < strlen(token); i++)
    {
        if (token[i] == '[')
        {
            j=i+1;
            while(token[j] != ']'){
                arg[k] = token[j];
                j++;
                k++;
            }
            arg[k] = '\0';
            isArray = TRUE;
            break;
        }
    }
    
    if(isArray == FALSE)
        return(0);
    
    if(checkIfInteger(arg) == FALSE){
        return(1);
    }
    
    return(0);
}


int check_if_global_var( char *token )  {
   if(token[0] == '$') {
        if ( isalpha(token[1]) ) {
                    
            
        /*
        * insert here more checking
        */	
        }      
        else {
            return (1);  
        }
   }
   else{
       return (1);
}
 
   return (0);
}


int check_if_var( char *token )  {
   if(token[0] == '$'){
        if ( isalpha(token[1]) ) {
        /*
        * insert here more checking
        */
        }      
        else {
            return (0);  
        }
    }
    else {
        return (0);
    }
 
    return (1);
	
}


int free_var_mem(khash_t(khStrInt) *h){
    khiter_t k;
    const char *key;
    
    for (k = 0; k < kh_end(h); ++k)
        if (kh_exist(h, k)){
            key = kh_key(h,k);
            if(check_if_array(key ,h) == TRUE)
                free((char*)kh_key(h, k));
        }
    return(0);
}

int check_if_should_wake(struct plist *curr){
    clock_t now = clock();
    if(( (double)(now - curr->startSleepTime) / CLOCKS_PER_SEC) >= curr->secOfSleep){
        return(TRUE);
    }
    
    return(FALSE);
}

int checkSyntax(char token[MAX_WORDS_PER_LINE][MAX_WORD_LENGTH], khash_t(khStrInt) *h_loc){  //inline int checkSyntax || inline kai tis mikres sun?

    int j, r;
    khiter_t k;
    char instruction[MAX_WORD_LENGTH];
    
    //initialize
    memset(instruction, '\0', sizeof(instruction));

    
    strcpy(instruction, token[0]);
 
   
    if(strcmp(instruction, "LOAD") == 0){
        // 1st parameter: var
        CHECK_IF_VAR( token[1] )
        if( check_if_array(token[1], h_loc) == 0){
            kh_put(khStrInt, h_loc, token[1], &r);
            if(r == 1)
                kh_set(khStrInt, h_loc, token[1], UNINITIALIZED); //kh_put in memory (no initialize)
        }
        // 2nd parameter: globalVar
        CHECK_IF_GLOBAL_VAR( token[2] )
        if(check_if_array(token[2], h_glob) == 0){
            kh_put(khStrInt, h_glob, token[2], &r);
            if(r == 1)
                kh_set(khStrInt, h_glob, token[2], UNINITIALIZED); //kh_put in memory (no initialize)
        }
        //elegxos stin periptwsi: LOAD $r1 $r2 $r3
        if(token[3][0] != '\0')
            return(1);
        
    }
    else if(strcmp(instruction, "STORE") == 0){
        // 1st parameter: globalVar
        CHECK_IF_GLOBAL_VAR( token[1] )
        if(check_if_array(token[1], h_glob) == 0){
            kh_put(khStrInt, h_glob, token[1], &r);
            if(r == 1)
                kh_set(khStrInt, h_glob, token[1], UNINITIALIZED); //kh_put in memory (no initialize)
        }
        // 2nd parameter: var or int. 
        CHECK_IF_VARVAL(token[2])
        if(checkIfInteger(token[2]) == FALSE){
            if(check_if_array(token[2], h_loc) == 0){
                kh_put(khStrInt, h_loc, token[2], &r);
                if(r == 1)
                    kh_set(khStrInt, h_loc, token[2], UNINITIALIZED); //kh_put in memory (no initialize)
            }
        }
            
        // elegxos stin periptwsi: STORE $r1 $r2 $r3
        if(token[3][0] != '\0')
            return(1);
            
    }
    else if(strcmp(instruction, "SET") == 0){
            // 1st parameter: var
        CHECK_IF_VAR( token[1] )
        if(check_if_array(token[1], h_loc) == 0) {
            kh_put(khStrInt, h_loc, token[1], &r);
            if(r == 1)
                kh_set(khStrInt, h_loc, token[1], UNINITIALIZED); //kh_put in memory (no initialize)
        } 
        
        // 2nd parameter: var or int. 
        CHECK_IF_VARVAL(token[2])
        if(checkIfInteger(token[2]) == FALSE){
            if(check_if_array(token[2], h_loc) == 0){
                kh_put(khStrInt, h_loc, token[2], &r);
                if(r == 1)
                    kh_set(khStrInt, h_loc, token[2], UNINITIALIZED); //kh_put in memory (no initialize)
            }
        }
        // elegxos stin periptwsi: SET $r1 $r2 $r3
        if(token[3][0] != '\0')
            return(1);
        
    }
    else if(strcmp(instruction, "ADD") == 0){
        // 1st parameter: var
        CHECK_IF_VAR( token[1] )
        if( check_if_array(token[1], h_loc) == 0){
            kh_put(khStrInt, h_loc, token[1], &r);
            if(r == 1)
                kh_set(khStrInt, h_loc, token[1], UNINITIALIZED); //kh_put in memory (no initialize)
        }
        
        // 2nd parameter: var or int. 
        CHECK_IF_VARVAL(token[2])
        if(checkIfInteger(token[2]) == FALSE){
            if(check_if_array(token[2], h_loc) == 0){
                kh_put(khStrInt, h_loc, token[2], &r);
                if(r == 1)
                    kh_set(khStrInt, h_loc, token[2], UNINITIALIZED);
            }
        }
        
        // 3rd parameter: var or int. 
        CHECK_IF_VARVAL(token[3])   
        if(checkIfInteger(token[3]) == FALSE){
            if(check_if_array(token[3], h_loc) == 0){
                kh_put(khStrInt, h_loc, token[3], &r);
                if(r == 1)
                    kh_set(khStrInt, h_loc, token[3], UNINITIALIZED);
            }
        }
        // elegxos stin periptwsi: ADD $r1 $r2 $r3 $r4
        if(token[4][0] != '\0')
                return(1);
        
    }
    else if(strcmp(instruction, "SUB") == 0){
          // 1st parameter: var
        CHECK_IF_VAR( token[1] )
        if( check_if_array(token[1], h_loc) == 0){
            kh_put(khStrInt, h_loc, token[1], &r);
            if(r == 1)
                kh_set(khStrInt, h_loc, token[1], UNINITIALIZED); //kh_put in memory (no initialize)
        }
        
        // 2nd parameter: var or int. 
        CHECK_IF_VARVAL(token[2])
        if(checkIfInteger(token[2]) == FALSE){
            if(check_if_array(token[2], h_loc) == 0){
                kh_put(khStrInt, h_loc, token[2], &r);
                if(r == 1)
                    kh_set(khStrInt, h_loc, token[2], UNINITIALIZED);
            }
        }
        
        // 3rd parameter: var or int. 
        CHECK_IF_VARVAL(token[3])   
        if(checkIfInteger(token[3]) == FALSE){
            if(check_if_array(token[3], h_loc) == 0){
                kh_put(khStrInt, h_loc, token[3], &r);
                if(r == 1)
                    kh_set(khStrInt, h_loc, token[3], UNINITIALIZED);
            }
        }
        // elegxos stin periptwsi: SUB $r1 $r2 $r3 $r4
        if(token[4][0] != '\0')
                return(1);       
        
    }
    else if(strcmp(instruction, "MUL") == 0){
           // 1st parameter: var
        CHECK_IF_VAR( token[1] )
        if( check_if_array(token[1], h_loc) == 0)  {
            kh_put(khStrInt, h_loc, token[1], &r);
            if(r == 1)
                kh_set(khStrInt, h_loc, token[1], UNINITIALIZED); //kh_put in memory (no initialize)
        }
        
        // 2nd parameter: var or int. 
        CHECK_IF_VARVAL(token[2])
        if(checkIfInteger(token[2]) == FALSE){
            if(check_if_array(token[2], h_loc) == 0){
                kh_put(khStrInt, h_loc, token[2], &r);
                if(r == 1)
                    kh_set(khStrInt, h_loc, token[2], UNINITIALIZED);
            }
        }
        
        // 3rd parameter: var or int. 
        CHECK_IF_VARVAL(token[3])   
        if(checkIfInteger(token[3]) == FALSE){
            if(check_if_array(token[3], h_loc) == 0){
                kh_put(khStrInt, h_loc, token[3], &r);
                if(r == 1)
                    kh_set(khStrInt, h_loc, token[3], UNINITIALIZED);
            }
        }
        // elegxos stin periptwsi: MUL $r1 $r2 $r3 $r4
        if(token[4][0] != '\0')
                return(1);
    }
    else if(strcmp(instruction, "DIV") == 0){
            // 1st parameter: var
        CHECK_IF_VAR( token[1] )
        if( check_if_array(token[1], h_loc) == 0) {
            kh_put(khStrInt, h_loc, token[1], &r);
            if(r == 1)
                kh_set(khStrInt, h_loc, token[1], UNINITIALIZED); //kh_put in memory (no initialize)
        }
        
        // 2nd parameter: var or int. 
        CHECK_IF_VARVAL(token[2])
        if(checkIfInteger(token[2]) == FALSE){
            if(check_if_array(token[2], h_loc) == 0) {
                kh_put(khStrInt, h_loc, token[2], &r);
                if(r == 1)
                    kh_set(khStrInt, h_loc, token[2], UNINITIALIZED);
            }
        }
        
        // 3rd parameter: var or int. 
        CHECK_IF_VARVAL(token[3])   
        if(checkIfInteger(token[3]) == FALSE){
            if(check_if_array(token[3], h_loc) == 0) {
                kh_put(khStrInt, h_loc, token[3], &r);
                if(r == 1)
                    kh_set(khStrInt, h_loc, token[3], UNINITIALIZED);
            }
        }
        // elegxos stin periptwsi: DIV $r1 $r2 $r3 $r4
        if(token[4][0] != '\0')
                return(1);
    }
    else if(strcmp(instruction, "MOD") == 0){
             // 1st parameter: var
        CHECK_IF_VAR( token[1] )
        if( check_if_array(token[1], h_loc) == 0) {
            kh_put(khStrInt, h_loc, token[1], &r);
            if(r == 1)
                kh_set(khStrInt, h_loc, token[1], UNINITIALIZED); //kh_put in memory (no initialize)
        }
        
        // 2nd parameter: var or int. 
        CHECK_IF_VARVAL(token[2])
        if(checkIfInteger(token[2]) == FALSE){
            if(check_if_array(token[2], h_loc) == 0) {
                kh_put(khStrInt, h_loc, token[2], &r);
                if(r == 1)
                    kh_set(khStrInt, h_loc, token[2], UNINITIALIZED);
            }
        }
        
        // 3rd parameter: var or int. 
        CHECK_IF_VARVAL(token[3])   
        if(checkIfInteger(token[3]) == FALSE){
            if(check_if_array(token[3], h_loc) == 0) {
                kh_put(khStrInt, h_loc, token[3], &r);
                if(r == 1)
                    kh_set(khStrInt, h_loc, token[3], UNINITIALIZED);
            }
        }
        // elegxos stin periptwsi: MOD $r1 $r2 $r3 $r4
        if(token[4][0] != '\0')
                return(1);
    }
    else if(strcmp(instruction, "BRGT") == 0){
        // 1st parameter: var or int. 
        CHECK_IF_VARVAL(token[1])
        if(checkIfInteger(token[1]) == FALSE){
            if(check_if_array(token[1], h_loc) == 0){
            kh_put(khStrInt, h_loc, token[1], &r);
            if(r == 1)
                kh_set(khStrInt, h_loc, token[1], UNINITIALIZED); //kh_put in memory (no initialize)
            }
        }
        
        // 2nd parameter: var or int. 
        CHECK_IF_VARVAL(token[2])
        if(checkIfInteger(token[2]) == FALSE){
            if(check_if_array(token[2], h_loc) == 0){
                kh_put(khStrInt, h_loc, token[2], &r);
                if(r == 1)
                    kh_set(khStrInt, h_loc, token[2], UNINITIALIZED); 
            }
        }        
        // elegxos stin periptwsi: $r1 $r2 label $r3    
        if(token[4][0] != '\0')
                return(1);

    }
    else if(strcmp(instruction, "BRGE") == 0){
         // 1st parameter: var or int. 
        CHECK_IF_VARVAL(token[1])
        if(checkIfInteger(token[1]) == FALSE){
            if(check_if_array(token[1], h_loc) == 0){
            kh_put(khStrInt, h_loc, token[1], &r);
            if(r == 1)
                kh_set(khStrInt, h_loc, token[1], UNINITIALIZED); //kh_put in memory (no initialize)
            }
        }
        
        // 2nd parameter: var or int. 
        CHECK_IF_VARVAL(token[2])
        if(checkIfInteger(token[2]) == FALSE){
            if(check_if_array(token[2], h_loc) == 0){
                kh_put(khStrInt, h_loc, token[2], &r);
                if(r == 1)
                    kh_set(khStrInt, h_loc, token[2], UNINITIALIZED); 
            }
        }        
        // elegxos stin periptwsi: $r1 $r2 label $r3    
        if(token[4][0] != '\0')
                return(1);
    }
    else if(strcmp(instruction, "BRLT") == 0){
         // 1st parameter: var or int. 
        CHECK_IF_VARVAL(token[1])
        if(checkIfInteger(token[1]) == FALSE){
            if(check_if_array(token[1], h_loc) == 0)  {
            kh_put(khStrInt, h_loc, token[1], &r);
            if(r == 1)
                kh_set(khStrInt, h_loc, token[1], UNINITIALIZED); //kh_put in memory (no initialize)
            }
        }
        
        // 2nd parameter: var or int. 
        CHECK_IF_VARVAL(token[2])
        if(checkIfInteger(token[2]) == FALSE){
            if(check_if_array(token[2], h_loc) == 0) {
                kh_put(khStrInt, h_loc, token[2], &r);
                if(r == 1)
                    kh_set(khStrInt, h_loc, token[2], UNINITIALIZED); 
            }
        }        
        // elegxos stin periptwsi: $r1 $r2 label $r3    
        if(token[4][0] != '\0')
                return(1);
    }
    else if(strcmp(instruction, "BRLE") == 0){
         // 1st parameter: var or int. 
        CHECK_IF_VARVAL(token[1])
        if(checkIfInteger(token[1]) == FALSE){
            if(check_if_array(token[1], h_loc) == 0) {
            kh_put(khStrInt, h_loc, token[1], &r);
            if(r == 1)
                kh_set(khStrInt, h_loc, token[1], UNINITIALIZED); //kh_put in memory (no initialize)
            }
        }
        
        // 2nd parameter: var or int. 
        CHECK_IF_VARVAL(token[2])
        if(checkIfInteger(token[2]) == FALSE){
            if(check_if_array(token[2], h_loc) == 0){
                kh_put(khStrInt, h_loc, token[2], &r);
                if(r == 1)
                    kh_set(khStrInt, h_loc, token[2], UNINITIALIZED); 
            }
        }        
        // elegxos stin periptwsi: $r1 $r2 label $r3    
        if(token[4][0] != '\0')
                return(1);
    }
    else if(strcmp(instruction, "BREQ") == 0){
        // 1st parameter: var or int. 
        CHECK_IF_VARVAL(token[1])
        if(checkIfInteger(token[1]) == FALSE){
            if(check_if_array(token[1], h_loc) == 0)  {
            kh_put(khStrInt, h_loc, token[1], &r);
            if(r == 1)
                kh_set(khStrInt, h_loc, token[1], UNINITIALIZED); //kh_put in memory (no initialize)
            }
        }
        
        // 2nd parameter: var or int. 
        CHECK_IF_VARVAL(token[2])
        if(checkIfInteger(token[2]) == FALSE){
            if(check_if_array(token[2], h_loc) == 0)  {
                kh_put(khStrInt, h_loc, token[2], &r);
                if(r == 1)
                    kh_set(khStrInt, h_loc, token[2], UNINITIALIZED); 
            }
        }        
        // elegxos stin periptwsi: $r1 $r2 label $r3    
        if(token[4][0] != '\0')
                return(1);
    }
    else if(strcmp(instruction, "BRA") == 0){
        
        // elegxos stin periptwsi: BRA label $r1   
        if(token[2][0] != '\0')
                return(1);
    }
    else if(strcmp(instruction, "DOWN") == 0){
        CHECK_IF_GLOBAL_VAR(token[1])
        if(check_if_array(token[1], h_glob) == 0){
            kh_put(khStrInt, h_glob, token[1], &r);
            if(r == 1)
                kh_set(khStrInt, h_glob, token[1], UNINITIALIZED); //kh_put in memory (no initialize)
        }
        
        //elegxos stin periptwsi: DOWN $r1 $r2      
        if(token[2][0] != '\0')
                return(1);
    }
    else if(strcmp(instruction, "UP") == 0){
        CHECK_IF_GLOBAL_VAR(token[1])
        if(check_if_array(token[1], h_glob) == 0){
            kh_put(khStrInt, h_glob, token[1], &r);
            if(r == 1)
                kh_set(khStrInt, h_glob, token[1], UNINITIALIZED); //kh_put in memory (no initialize)
        }
        
        // elegxos stin periptwsi: UP $r1 $r2    
        if(token[2][0] != '\0')
                return(1);
        
    }
    else if(strcmp(instruction, "SLEEP") == 0){
        
        // 1st parameter: var or int. 
        CHECK_IF_VARVAL(token[1])
        if(check_if_array(token[1], h_loc) == 0)  {
            kh_put(khStrInt, h_loc, token[1], &r);
            if(r == 1)
                kh_set(khStrInt, h_loc, token[1], UNINITIALIZED); //put in memory (no initialize)
        }
        // elegxos stin periptwsi: SLEEP $r1 $r2    
        if(token[2][0] != '\0')
                return(1);
    }
    else if(strcmp(instruction, "PRINT") == 0){
        for(j=2; token[j][0] != '\0'; j++){ 
            if(checkIfInteger(token[j]) == FALSE){
                CHECK_IF_VARVAL(token[j])
                if(check_if_array(token[j], h_loc) == 0){
                    kh_put(khStrInt, h_loc, token[j], &r);
                    if(r == 1)
                        kh_set(khStrInt, h_loc, token[j], UNINITIALIZED);
                }
            }
        }
    }
    else if(strcmp(instruction, "RETURN") == 0){
         if(token[1][0] != '\0')
                return(1);
    }
    else if(strcmp(instruction, "#PROGRAM") == 0){
        if(token[1][0] != '\0')
                return(1);
    }
    else{
        return(1);
    }
    
    
    return(0);
}


int compile(struct plist *curr){

    int i, j, r, 
        pc,
        totalLines,
        labelCount,
        t_id;
    //int **args;
    char line[MAX_LINE_LENGTH];     
    char temp_token[MAX_WORD_LENGTH];
    char argv[MAX_WORDS_PER_LINE][MAX_WORD_LENGTH],
         *temp_string;      //prosorini metabliti gia iteration metaksi tokens
    char temp_argv_str[MAX_WORD_LENGTH],
         temp_num_str[MAX_WORD_LENGTH],
         temp_pr[MAX_PRINT_LENGTH]; //prosorini metabliti gia print instruction

         
    //initialize
    for(j=0; j < MAX_WORDS_PER_LINE; j++){
        memset(argv[j], '\0', sizeof(argv[j]));
    }

    int argc = curr->argc;
    for(j=0; j <= argc; j++) 
        strcpy(argv[j], curr->argv[j]);
         
    /*** hash table*******/
    khiter_t k; 
    khash_t(khStrInt) *h_loc = kh_init(khStrInt); // create a hashtable
    
    //thread id
    t_id = atoi(argv[0]);
    
    //metrame grammes arxeiou
    totalLines = countLines(argv[1]); 
    if(totalLines == -1){
        printf("(Thread id: %d) Error getting num of lines from file.\n", t_id);
        free_var_mem(h_loc);
        kh_destroy(khStrInt, h_loc);
        return -1;
    }
    char token[totalLines][MAX_WORDS_PER_LINE][MAX_WORD_LENGTH],
         labels[totalLines][MAX_WORD_LENGTH]; 
    
   /******initialize memory *******/
    memset(temp_pr, '\0', sizeof(temp_pr));
    memset(temp_token, '\0', sizeof(temp_token));
    memset(temp_argv_str, '\0', sizeof(temp_argv_str));
    for(j=0; j < totalLines; j++){
        memset(token[j], '\0', sizeof(token[j]));
        memset(labels[j], '\0', sizeof(labels[j]));
    }
    
    
	FILE* fp = fopen(argv[1], "r");
    if(fp == NULL) {
      printf("(Thread id: %d) Error opening file", t_id); 
      free_var_mem(h_loc);
      kh_destroy(khStrInt, h_loc);
      return -1;
   }
   
   //put arguments in the right place
    strcpy(argv[1], argv[0]);
    for(i=1; i < argc; i++){
        strcpy(argv[i-1], argv[i]);    
    }
    //argument count 
    argc = argc - 1;
    
    
    //insert arguments with values in memory
    for(i=0;i < argc;i++){
        if(checkIfInteger(argv[i]) == FALSE){
            printf("(Thread id: %d) Invalid line argument. They can only be integers\n", t_id);
            free_var_mem(h_loc);
            kh_destroy(khStrInt, h_loc);
            fclose(fp);
            return -1;
        }
        //convert $argv[i] to string in order to put in memory
        strcpy(temp_argv_str,  "$argv[");
        sprintf(temp_num_str, "%d", i);
        strcat(temp_argv_str, temp_num_str);
        strcat(temp_argv_str, "]");
        //debugging
        printf("temp_argv_str: %s\n", temp_argv_str);
        
        k = kh_put(khStrInt, h_loc, temp_argv_str, &r);
        if(r)
            kh_key(h_loc, k) = strdup(temp_argv_str);
        kh_set(khStrInt, h_loc, temp_argv_str, atoi(argv[i]));
        
        printf("thread id: %d argc: %d argv[%d] = %s\n", t_id, argc, i, argv[i]);
    }
    kh_set(khStrInt, h_loc, "$argc", argc);
    
    
    
 printf("\nTokens of user program code:\n");
/******************** COMPILE **********************************************************/
    pc = 0; //program counter
    labelCount = 0;
    while (pc < totalLines){ //for every line
        
        if (fgets(line, 200, fp) == NULL) 
            break;  //end of file
             
        /***** TOKENS ON THE SAME LINE********/
        i = 0; //iteration between tokens
        temp_string = strtok(line, s);  // get the first token 
        while( temp_string != NULL ) {  // walk through other tokens on the same line  
            strcpy(temp_token, temp_string);
            
            //case "print"
            if(temp_token[0] == '\"'){    
                strcpy(temp_pr, strtok(NULL, p));
                
                for(j = 1; temp_token[j] != '\0'; ++j){ //delete-> "
                    temp_token[j-1] = temp_token[j];
                }
                temp_token[j-1] = ' ';
                for(k = 0; temp_pr[k] != '\0'; ++k, ++j){ //olokliri print se 1 token
                    temp_token[j] = temp_pr[k];
                }
                temp_token[j] = '\0';
            }
            
            //assing 
            strcpy(token[pc][i] ,temp_token);
            
            //next token on the line
            i++;
            temp_string = strtok(NULL, s);
        }
        
        
        /***** STORE LABEL OF THE LINE ******************/
        if((token[pc][0][0] == 'L') && (strcmp(token[pc][0], "LOAD") != 0)){
                
            strcpy(labels[labelCount], token[pc][0]);
            //value of pc at the label
            kh_put(khStrInt, h_loc, labels[labelCount], &r);
            if(r == 0){
                printf("(Thread id: %d) There can only unique labels. Error line (%d)\n", t_id, pc+1); 
                free_var_mem(h_loc);
                kh_destroy(khStrInt, h_loc);
                fclose(fp);
                return -1;
            }
            kh_set(khStrInt, h_loc,labels[labelCount], pc);
            labelCount++;
            
            //delete label from instruction
            for(j=1; token[pc][j][0] != '\0'; j++){
                strcpy(token[pc][j-1], token[pc][j]);
            }
            memset(token[pc][j-1], '\0', sizeof(token[pc][j-1]));        
        }

        
        /***** CHECK SYNTAX ERROR FOR THE LINE *********/
        if(checkSyntax(token[pc], h_loc)){ 
            printf("(Thread id: %d) There is a syntax error at line (%d). Exiting.\n", t_id, pc+1); 
            free_var_mem(h_loc);
            kh_destroy(khStrInt, h_loc);
            fclose(fp);
            return -1;
        }
        
        
        //debugging print 3
        for(j=0; token[pc][j][0] != '\0'; j++){
            printf("%d: %s\n", j, token[pc][j]);
        }
        
        
        
        //next line
        pc++;
        printf("*****************New line****************\n");
    }

    if(strcmp(token[0][0], "#PROGRAM")){
        printf("(Thread id: %d) Wrong syntax at line 0. \"#PROGRAM\" is needed. Exiting\n", t_id);
        free_var_mem(h_loc);
        kh_destroy(khStrInt, h_loc);
        fclose(fp);
        return -1;
    }

    
    /*************debug compile*****************************************/
    
    int tval;
printf("size: %d\tbuckets: %d\n", kh_size(h_loc), kh_n_buckets(h_loc));
printf("\nIeterate all LOCAL keys\n");
   for (k = kh_begin(h_loc); k != kh_end(h_loc); ++k) {
      if (kh_exist(h_loc, k)) {
         const char *key = kh_key(h_loc,k);
         //kh_value(h_loc, k) = UNINITIALIZED;
         tval = kh_value(h_loc, k);
         printf("key=%s  val=%d\n", key, tval);
      }
   }
printf("\nIeterate all GLOBAL keys\n");
   for (k = kh_begin(h_glob); k != kh_end(h_glob); ++k) {
      if (kh_exist(h_glob, k)) {
         const char *key = kh_key(h_glob,k);
         //kh_value(h_glob, k) = UNINITIALIZED;
         tval = kh_value(h_glob, k);
         printf("key=%s  val=%d\n", key, tval);
      }
   }
    
    curr->argc = argc;
    for(j=0; j <= argc; j++) 
        strcpy(curr->argv[j], argv[j]);
    curr->h_loc = h_loc;
    for(i=0; token[i][0][0] != '\0'; i++){
        for(j=0; token[i][j][0] != '\0'; j++){
            strcpy(curr->token[i][j], token[i][j]);
        }
    }
    
    return(0);
}
    
    
/********************** RUN **********************************************************/
   
int run(struct plist *curr){

    int i, j, r, 
        argc,
        pc,
        totalLines,
        t_id,
        counter = 0,
        tval,
        value,
        value2;
    char argv[MAX_WORDS_PER_LINE][MAX_WORD_LENGTH],
         instruction[MAX_WORD_LENGTH],
         tempTokenArray[MAX_WORD_LENGTH],
         arg1[MAX_WORD_LENGTH],
         arg2[MAX_WORD_LENGTH],
         arg3[MAX_WORD_LENGTH];
        
         
    //initialize
    for(j=0; j < MAX_WORDS_PER_LINE; j++){
        memset(argv[j], '\0', sizeof(argv[j]));
    }   
         
    argc = curr->argc;
    for(j=0; j <= argc; j++) 
        strcpy(argv[j], curr->argv[j]);
    pc = curr->pc;
        
    /*** hash table*******/
    khiter_t k; 
    khash_t(khStrInt) *h_loc = curr->h_loc;
    
    //thread id
    t_id = atoi(argv[0]);
    
    //metrame grammes arxeiou
    totalLines = countLines(argv[1]); 
    if(totalLines == -1){
        printf("(Thread id: %d) Error getting num of lines from file.\n", t_id);
        free_var_mem(h_loc);
        kh_destroy(khStrInt, h_loc);
        return -1;
    }
    char token[totalLines][MAX_WORDS_PER_LINE][MAX_WORD_LENGTH];
    
   /******initialize memory *******/
    memset(instruction, '\0', sizeof(instruction));
    memset(tempTokenArray, '\0', sizeof(tempTokenArray));
    memset(arg1, '\0', sizeof(arg1));
    memset(arg2, '\0', sizeof(arg2));
    memset(arg3, '\0', sizeof(arg3));
    for(j=0; j < totalLines; j++){
        memset(token[j], '\0', sizeof(token[j]));
    }

    for(i=0; token[i][0][0] != '\0'; i++){
        for(j=0; token[i][j][0] != '\0'; j++){
            strcpy(token[i][j], curr->token[i][j]);
        }
    }
        
        




    while(1){
        
        if(counter >= NUM_INSTR_CHANGE){
            curr->pc = pc;
            return(1);
        }
        
        strcpy(instruction, token[pc][0]);
        if(token[pc][1][0] != '\0'){
            strcpy(arg1, token[pc][1]);
            if(token[pc][2][0] != '\0'){
                strcpy(arg2, token[pc][2]);
                if(token[pc][3][0] != '\0')
                    strcpy(arg3, token[pc][3]);
            }
        }
        

        if(strcmp(instruction, "LOAD") == 0){
            //multithread prob
            //2nd argument 
            if(check_if_array(arg2, h_glob) == TRUE){
                if(getTempTokenArray(arg2, h_glob) == -1){
                    printf("(Thread id: %d) Wrong at run time. Line: (%d). Variable inside array is not initialized\n",t_id, pc);
                    free_var_mem(h_loc);
                    kh_destroy(khStrInt, h_loc);
                     
                    return -1;
                }     
                k = kh_put(khStrInt, h_glob, arg2, &r);
                if(r)
                    kh_key(h_glob, k) = strdup(arg2);
            
            }

            value = kh_get_val(khStrInt, h_glob, arg2, UNINITIALIZED);
            if(value == UNINITIALIZED){
                printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable not initialized\n",t_id, pc+1);
                free_var_mem(h_loc);
                kh_destroy(khStrInt, h_loc);
                 
                return -1;
            }   
            
            //1st argument     
            if(check_if_array(arg1, h_loc) == TRUE){
                if(getTempTokenArray(arg1, h_loc) == -1){
                    printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable inside array is not initialized\n",t_id, pc);
                    free_var_mem(h_loc);
                    kh_destroy(khStrInt, h_loc);
                     
                    return -1;
                }      
                k = kh_put(khStrInt, h_loc, arg1, &r);
                if(r)
                    kh_key(h_loc, k) = strdup(arg1);
            }
            
            kh_set(khStrInt, h_loc, arg1, value);
        }
        else if(strcmp(instruction, "STORE") == 0){
            //multithread prob
            //2nd argument 
            if(check_if_array(arg2, h_loc) == TRUE){
                if(getTempTokenArray(arg2, h_loc) == -1){
                    printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable inside array is not initialized\n",t_id, pc);
                    free_var_mem(h_loc);
                    kh_destroy(khStrInt, h_loc);
                     
                    return -1;
                }     
                k = kh_put(khStrInt, h_loc, arg2, &r);
                if(r)
                    kh_key(h_loc, k) = strdup(arg2);
            
            }

            value = kh_get_val(khStrInt, h_loc, arg2, UNINITIALIZED);
            if(value == UNINITIALIZED){
                printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable not initialized\n",t_id, pc+1);
                free_var_mem(h_loc);
                kh_destroy(khStrInt, h_loc);
                 
                return -1;
            }  
            
            //1st argument      
            if(check_if_array(arg1, h_glob) == TRUE){
                if(getTempTokenArray(arg1, h_glob) == -1){
                    printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable inside array is not initialized\n",t_id, pc);
                    kh_destroy(khStrInt, h_glob);
                     
                    return -1;
                }      
                k = kh_put(khStrInt, h_glob, arg1, &r);
                if(r)
                    kh_key(h_loc, k) = strdup(arg1);
            }
            kh_set(khStrInt, h_glob, arg1, value);
            
        }
        else if(strcmp(instruction, "SET") == 0){
            //arg2
            if(check_if_var(arg2)){
                if(check_if_array(arg2, h_loc) == TRUE){
                    if(getTempTokenArray(arg2, h_loc) == -1){
                        printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable inside array is not initialized\n",t_id, pc);
                        free_var_mem(h_loc);
                        kh_destroy(khStrInt, h_loc);
                         
                        return -1;
                    }     
                    k = kh_put(khStrInt, h_loc, arg2, &r);
                    if(r)
                        kh_key(h_loc, k) = strdup(arg2);
                }
                value = kh_get_val(khStrInt, h_loc, arg2, UNINITIALIZED);
            }else{
                value = atoi(arg2);
            }
            if(value == UNINITIALIZED){
                printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable not initialized\n",t_id, pc+1);
                free_var_mem(h_loc);
                kh_destroy(khStrInt, h_loc);
                 
                return -1;
            }  
            //arg1
            if(check_if_var(arg1)){
                if(check_if_array(arg1, h_loc) == TRUE){
                    if(getTempTokenArray(arg1, h_loc) == -1){
                        printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable inside array is not initialized\n",t_id, pc);
                        free_var_mem(h_loc);
                        kh_destroy(khStrInt, h_loc);
                         
                        return -1;
                    }     
                    k = kh_put(khStrInt, h_loc, arg1, &r);
                    if(r)
                        kh_key(h_loc, k) = strdup(arg1);
                    }
            }
            kh_set(khStrInt, h_loc, arg1, value);
        }
        else if(strcmp(instruction, "ADD") == 0){
            //arg2
            if(check_if_var(arg2)){
                if(check_if_array(arg2, h_loc) == TRUE){
                    if(getTempTokenArray(arg2, h_loc) == -1){
                        printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable inside array is not initialized\n",t_id, pc);
                        free_var_mem(h_loc);
                        kh_destroy(khStrInt, h_loc);
                         
                        return -1;
                    }     
                    k = kh_put(khStrInt, h_loc, arg2, &r);
                    if(r)
                        kh_key(h_loc, k) = strdup(arg2);
            
                }
                value = kh_get_val(khStrInt, h_loc, arg2, UNINITIALIZED);
            }else{
                value = atoi(arg2);
            }
            if(value == UNINITIALIZED){
                printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable not initialized\n",t_id, pc+1);
                free_var_mem(h_loc);
                kh_destroy(khStrInt, h_loc);
                 
                return -1;
            } 
            //arg3
            if(check_if_var(arg3)){
                if(check_if_array(arg3, h_loc) == TRUE){
                    if(getTempTokenArray(arg3, h_loc) == -1){
                        printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable inside array is not initialized\n",t_id, pc);
                        free_var_mem(h_loc);
                        kh_destroy(khStrInt, h_loc);
                         
                        return -1;
                    }     
                    k = kh_put(khStrInt, h_loc, arg3, &r);
                    if(r)
                        kh_key(h_loc, k) = strdup(arg3);
                }
                value2 = kh_get_val(khStrInt, h_loc, arg3, UNINITIALIZED);
            }else{
                value2 = atoi(arg3);
            }
            if(value2== UNINITIALIZED){
                printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable not initialized\n",t_id, pc+1);
                free_var_mem(h_loc);
                kh_destroy(khStrInt, h_loc);
                 
                return -1;
            } 
            //arg1
            if(check_if_array(arg1, h_loc) == TRUE){
                if(getTempTokenArray(arg1, h_loc) == -1){
                    printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable inside array is not initialized\n",t_id, pc);
                    free_var_mem(h_loc);
                    kh_destroy(khStrInt, h_loc);
                     
                    return -1;
                }     
                k = kh_put(khStrInt, h_loc, arg1, &r);
                if(r)
                    kh_key(h_loc, k) = strdup(arg1);
            
            }
            kh_set(khStrInt, h_loc, arg1, (value + value2));
        }
        else if(strcmp(instruction, "SUB") == 0){
            //arg2
            if(check_if_var(arg2)){
                if(check_if_array(arg2, h_loc) == TRUE){
                    if(getTempTokenArray(arg2, h_loc) == -1){
                        printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable inside array is not initialized\n",t_id, pc);
                        free_var_mem(h_loc);
                        kh_destroy(khStrInt, h_loc);
                         
                        return -1;
                    }     
                    k = kh_put(khStrInt, h_loc, arg2, &r);
                    if(r)
                        kh_key(h_loc, k) = strdup(arg2);
                }
                value = kh_get_val(khStrInt, h_loc, arg2, UNINITIALIZED);
            }else{
                value = atoi(arg2);
            }
            if(value == UNINITIALIZED){
                printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable not initialized\n",t_id, pc+1);
                free_var_mem(h_loc);
                kh_destroy(khStrInt, h_loc);
                 
                return -1;
            } 
            //arg3
            if(check_if_var(arg3)){
                if(check_if_array(arg3, h_loc) == TRUE){
                    if(getTempTokenArray(arg3, h_loc) == -1){
                        printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable inside array is not initialized\n",t_id, pc);
                        free_var_mem(h_loc);
                        kh_destroy(khStrInt, h_loc);
                         
                        return -1;
                    }     
                    k = kh_put(khStrInt, h_loc, arg3, &r);
                    if(r)
                        kh_key(h_loc, k) = strdup(arg3);
                }   
                value2 = kh_get_val(khStrInt, h_loc, arg3, UNINITIALIZED);
            }else{
                value2 = atoi(arg3);
            }
            if(value2== UNINITIALIZED){
                printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable not initialized\n",t_id, pc+1);
                free_var_mem(h_loc);
                kh_destroy(khStrInt, h_loc);
                 
                return -1;
            } 
            //arg1
            if(check_if_array(arg1, h_loc) == TRUE){
                if(getTempTokenArray(arg1, h_loc) == -1){
                    printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable inside array is not initialized\n",t_id, pc);
                    free_var_mem(h_loc);
                    kh_destroy(khStrInt, h_loc);
                     
                    return -1;
                }     
                k = kh_put(khStrInt, h_loc, arg1, &r);
                if(r)
                    kh_key(h_loc, k) = strdup(arg1);
            }
            kh_set(khStrInt, h_loc, arg1, (value - value2));
        }
        else if(strcmp(instruction, "MUL") == 0){
            //arg2
            if(check_if_var(arg2)){
                if(check_if_array(arg2, h_loc) == TRUE){
                    if(getTempTokenArray(arg2, h_loc) == -1){
                        printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable inside array is not initialized\n",t_id, pc);
                        free_var_mem(h_loc);
                        kh_destroy(khStrInt, h_loc);
                         
                        return -1;
                    }     
                    k = kh_put(khStrInt, h_loc, arg2, &r);
                    if(r)
                        kh_key(h_loc, k) = strdup(arg2);
                }
                value = kh_get_val(khStrInt, h_loc, arg2, UNINITIALIZED);
            }else{
                value = atoi(arg2);
            }
            if(value == UNINITIALIZED){
                printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable not initialized\n",t_id, pc+1);
                free_var_mem(h_loc);
                kh_destroy(khStrInt, h_loc);
                 
                return -1;
            } 
            //arg3
            if(check_if_var(arg3)){
                if(check_if_array(arg3, h_loc) == TRUE){
                    if(getTempTokenArray(arg3, h_loc) == -1){
                        printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable inside array is not initialized\n",t_id, pc);
                        free_var_mem(h_loc);
                        kh_destroy(khStrInt, h_loc);
                         
                        return -1;
                    }     
                    k = kh_put(khStrInt, h_loc, arg3, &r);
                    if(r)
                        kh_key(h_loc, k) = strdup(arg3);
                }
                value2 = kh_get_val(khStrInt, h_loc, arg3, UNINITIALIZED);
            }else{
                value2 = atoi(arg3);
            }
            if(value2== UNINITIALIZED){
                printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable not initialized\n",t_id, pc+1);
                free_var_mem(h_loc);
                kh_destroy(khStrInt, h_loc);
                 
                return -1;
            } 
            //arg1
            if(check_if_array(arg3, h_loc) == TRUE){
                    if(getTempTokenArray(arg3, h_loc) == -1){
                        printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable inside array is not initialized\n",t_id, pc);
                        free_var_mem(h_loc);
                        kh_destroy(khStrInt, h_loc);
                         
                        return -1;
                    }     
                    k = kh_put(khStrInt, h_loc, arg3, &r);
                    if(r)
                        kh_key(h_loc, k) = strdup(arg3);
                }
            kh_set(khStrInt, h_loc, arg3, (value * value2));
        }
        else if(strcmp(instruction, "DIV") == 0){
            //arg2
            if(check_if_var(arg2)){
                if(check_if_array(arg2, h_loc) == TRUE){
                    if(getTempTokenArray(arg2, h_loc) == -1){
                        printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable inside array is not initialized\n",t_id, pc);
                        free_var_mem(h_loc);
                        kh_destroy(khStrInt, h_loc);
                         
                        return -1;
                    }     
                    k = kh_put(khStrInt, h_loc, arg2, &r);
                    if(r)
                        kh_key(h_loc, k) = strdup(arg2);
                }
                value = kh_get_val(khStrInt, h_loc, arg2, UNINITIALIZED);
            }else{
                value = atoi(arg2);
            }
            if(value == UNINITIALIZED){
                printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable not initialized\n",t_id, pc+1);
                free_var_mem(h_loc);
                kh_destroy(khStrInt, h_loc);
                 
                return -1;
            } 
            //arg3
            if(check_if_var(arg3)){
                if(check_if_array(arg3, h_loc) == TRUE){
                    if(getTempTokenArray(arg3, h_loc) == -1){
                        printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable inside array is not initialized\n",t_id, pc);
                        free_var_mem(h_loc);
                        kh_destroy(khStrInt, h_loc);
                         
                        return -1;
                    }     
                    k = kh_put(khStrInt, h_loc, arg3, &r);
                    if(r)
                        kh_key(h_loc, k) = strdup(arg3);
                }
                value2 = kh_get_val(khStrInt, h_loc, arg3, UNINITIALIZED);
            }else{
                value2 = atoi(arg3);
            }
            if(value2== UNINITIALIZED){
                printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable not initialized\n",t_id, pc+1);
                free_var_mem(h_loc);
                kh_destroy(khStrInt, h_loc);
                 
                return -1;
            } 
            //arg1
            if(check_if_array(arg1, h_loc) == TRUE){
                    if(getTempTokenArray(arg1, h_loc) == -1){
                        printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable inside array is not initialized\n",t_id, pc);
                        free_var_mem(h_loc);
                        kh_destroy(khStrInt, h_loc);
                         
                        return -1;
                    }     
                    k = kh_put(khStrInt, h_loc, arg1, &r);
                    if(r)
                        kh_key(h_loc, k) = strdup(arg1);
            }
            value = (int)(value / value2);
            kh_set(khStrInt, h_loc, arg1, value);
        }
        else if(strcmp(instruction, "MOD") == 0){
            //arg2
            if(check_if_var(arg2)){
                if(check_if_array(arg2, h_loc) == TRUE){
                    if(getTempTokenArray(arg2, h_loc) == -1){
                        printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable inside array is not initialized\n",t_id, pc);
                        free_var_mem(h_loc);
                        kh_destroy(khStrInt, h_loc);
                         
                        return -1;
                    }     
                    k = kh_put(khStrInt, h_loc, arg2, &r);
                    if(r)
                        kh_key(h_loc, k) = strdup(arg2);
                }
                value = kh_get_val(khStrInt, h_loc, arg2, UNINITIALIZED);
            }else{
                value = atoi(arg2);
            }
            if(value == UNINITIALIZED){
                printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable not initialized\n",t_id, pc+1);
                free_var_mem(h_loc);
                kh_destroy(khStrInt, h_loc);
                 
                return -1;
            } 
            //arg3
            if(check_if_var(arg3)){
                if(check_if_array(arg3, h_loc) == TRUE){
                    if(getTempTokenArray(arg3, h_loc) == -1){
                        printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable inside array is not initialized\n",t_id, pc);
                        free_var_mem(h_loc);
                        kh_destroy(khStrInt, h_loc);
                         
                        return -1;
                    }     
                    k = kh_put(khStrInt, h_loc, arg3, &r);
                    if(r)
                        kh_key(h_loc, k) = strdup(arg3);
                } 
                value2 = kh_get_val(khStrInt, h_loc, arg3, UNINITIALIZED);
            }else{
                value2 = atoi(arg3);
            }
            if(value2== UNINITIALIZED){
                printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable not initialized\n",t_id, pc+1);
                free_var_mem(h_loc);
                kh_destroy(khStrInt, h_loc);
                 
                return -1;
            } 
            //arg1
            if(check_if_array(arg1, h_loc) == TRUE){
                    if(getTempTokenArray(arg1, h_loc) == -1){
                        printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable inside array is not initialized\n",t_id, pc);
                        free_var_mem(h_loc);
                        kh_destroy(khStrInt, h_loc);
                         
                        return -1;
                    }     
                    k = kh_put(khStrInt, h_loc, arg1, &r);
                    if(r)
                        kh_key(h_loc, k) = strdup(arg1);
            }
            kh_set(khStrInt, h_loc, arg1, (value % value2));
            
        }
        else if(strcmp(instruction, "BRGT") == 0){
            //arg1
            if(check_if_var(arg1)){
                if(check_if_array(arg1, h_loc) == TRUE){
                    if(getTempTokenArray(arg1, h_loc) == -1){
                        printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable inside array is not initialized\n",t_id, pc);
                        free_var_mem(h_loc);
                        kh_destroy(khStrInt, h_loc);
                         
                        return -1;
                    }     
                    k = kh_put(khStrInt, h_loc, arg1, &r);
                    if(r)
                        kh_key(h_loc, k) = strdup(arg1);
                }
                value = kh_get_val(khStrInt, h_loc, arg1, UNINITIALIZED);
            }else{
                value = atoi(arg1);
            }
            if(value == UNINITIALIZED){
                printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable not initialized\n",t_id, pc+1);
                free_var_mem(h_loc);
                kh_destroy(khStrInt, h_loc);
                 
                return -1;
            } 
            //arg2
            if(check_if_var(arg2)){
                if(check_if_array(arg2, h_loc) == TRUE){
                    if(getTempTokenArray(arg2, h_loc) == -1){
                        printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable inside array is not initialized\n",t_id, pc);
                        free_var_mem(h_loc);
                        kh_destroy(khStrInt, h_loc);
                         
                        return -1;
                    }     
                    k = kh_put(khStrInt, h_loc, arg2, &r);
                    if(r)
                        kh_key(h_loc, k) = strdup(arg2);
                }
                value2 = kh_get_val(khStrInt, h_loc, arg2, UNINITIALIZED);
            }else{
                value2 = atoi(arg2);
            }
            if(value2 == UNINITIALIZED){
                printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable not initialized\n",t_id, pc+1);
                free_var_mem(h_loc);
                kh_destroy(khStrInt, h_loc);
                 
                return -1;
            } 
            //check lebel
            k = kh_get(khStrInt, h_loc, arg3);
            if(kh_exist(h_loc, k) == FALSE){
                printf("(Thread id: %d)  Wrong at run time. Line: (%d). Label doesn't exist\n",t_id, pc+1);
                free_var_mem(h_loc);
                kh_destroy(khStrInt, h_loc);
                 
                return -1; 
            }
            //compare
            if(value > value2){
                pc = kh_get_val(khStrInt, h_loc, arg3, -1);
                if(pc==-1){
                    printf("(Thread id: %d)  Wrong at run time. Line: (%d). Error at label.\n",t_id, pc+1);
                    free_var_mem(h_loc);
                    kh_destroy(khStrInt, h_loc);
                     
                    return -1; 
                }
                continue;
            }
        }
        else if(strcmp(instruction, "BRGE") == 0){
            //arg1
            if(check_if_var(arg1)){
                if(check_if_array(arg1, h_loc) == TRUE){
                    if(getTempTokenArray(arg1, h_loc) == -1){
                        printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable inside array is not initialized\n",t_id, pc);
                        free_var_mem(h_loc);
                        kh_destroy(khStrInt, h_loc);
                         
                        return -1;
                    }     
                    k = kh_put(khStrInt, h_loc, arg1, &r);
                    if(r)
                        kh_key(h_loc, k) = strdup(arg1);
                }
                value = kh_get_val(khStrInt, h_loc, arg1, UNINITIALIZED);
            }else{
                value = atoi(arg1);
            }
            if(value == UNINITIALIZED){
                printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable not initialized\n",t_id, pc+1);
                free_var_mem(h_loc);
                kh_destroy(khStrInt, h_loc);
                 
                return -1;
            } 
            //arg2
            if(check_if_var(arg2)){
                if(check_if_array(arg2, h_loc) == TRUE){
                    if(getTempTokenArray(arg2, h_loc) == -1){
                        printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable inside array is not initialized\n",t_id, pc);
                        free_var_mem(h_loc);
                        kh_destroy(khStrInt, h_loc);
                         
                        return -1;
                    }     
                    k = kh_put(khStrInt, h_loc, arg2, &r);
                    if(r)
                        kh_key(h_loc, k) = strdup(arg2);
                }
                value2 = kh_get_val(khStrInt, h_loc, arg2, UNINITIALIZED);
            }else{
                value2 = atoi(arg2);
            }
            if(value2 == UNINITIALIZED){
                printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable not initialized\n",t_id, pc+1);
                kh_destroy(khStrInt, h_loc);
                 
                return -1;
            } 
            //check lebel
            k = kh_get(khStrInt, h_loc, arg3);
            if(kh_exist(h_loc, k) == FALSE){
                printf("(Thread id: %d)  Wrong at run time. Line: (%d). Label doesn't exist\n",t_id, pc+1);
                kh_destroy(khStrInt, h_loc);
                 
                return -1; 
            }
            //compare
            if(value >= value2){
                pc = kh_get_val(khStrInt, h_loc, arg3, -1);
                if(pc==-1){
                    printf("(Thread id: %d)  Wrong at run time. Line: (%d). Error at label.\n",t_id, pc+1);
                    free_var_mem(h_loc);
                    kh_destroy(khStrInt, h_loc);
                     
                    return -1; 
                }
                continue;
            }
        }
        else if(strcmp(instruction, "BRLT") == 0){
            //arg1
            if(check_if_var(arg1)){
                if(check_if_array(arg1, h_loc) == TRUE){
                    if(getTempTokenArray(arg1, h_loc) == -1){
                        printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable inside array is not initialized\n",t_id, pc);
                        kh_destroy(khStrInt, h_loc);
                         
                        return -1;
                    }     
                    k = kh_put(khStrInt, h_loc, arg1, &r);
                    if(r)
                        kh_key(h_loc, k) = strdup(arg1);
                    }
                value = kh_get_val(khStrInt, h_loc, arg1, UNINITIALIZED);
            }else{
                value = atoi(arg1);
            }
            if(value == UNINITIALIZED){
                printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable not initialized\n",t_id, pc+1);
                free_var_mem(h_loc);
                kh_destroy(khStrInt, h_loc);
                 
                return -1;
            } 
            //arg2
            if(check_if_var(arg2)){
                if(check_if_array(arg2, h_loc) == TRUE){
                    if(getTempTokenArray(arg2, h_loc) == -1){
                        printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable inside array is not initialized\n",t_id, pc);
                        free_var_mem(h_loc);
                        kh_destroy(khStrInt, h_loc);
                         
                        return -1;
                    }     
                    k = kh_put(khStrInt, h_loc, arg2, &r);
                    if(r)
                        kh_key(h_loc, k) = strdup(arg2);
                }
                value2 = kh_get_val(khStrInt, h_loc, arg2, UNINITIALIZED);
            }else{
                value2 = atoi(arg2);
            }
            if(value2 == UNINITIALIZED){
                printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable not initialized\n",t_id, pc+1);
                free_var_mem(h_loc);
                kh_destroy(khStrInt, h_loc);
                 
                return -1;
            } 
            //check lebel
            k = kh_get(khStrInt, h_loc, arg3);
            if(kh_exist(h_loc, k) == FALSE){
                printf("(Thread id: %d)  Wrong at run time. Line: (%d). Label doesn't exist\n",t_id, pc+1);
                free_var_mem(h_loc);
                kh_destroy(khStrInt, h_loc);
                 
                return -1; 
            }
            //compare
            if(value < value2){
                pc = kh_get_val(khStrInt, h_loc, arg3, -1);
                if(pc==-1){
                    printf("(Thread id: %d)  Wrong at run time. Line: (%d). Error at label.\n",t_id, pc+1);
                    free_var_mem(h_loc);
                    kh_destroy(khStrInt, h_loc);
                     
                    return -1; 
                }
                continue;
            }
        }
        else if(strcmp(instruction, "BRLE") == 0){
            //arg1
            if(check_if_var(arg1)){
                if(check_if_array(arg1, h_loc) == TRUE){
                    if(getTempTokenArray(arg1, h_loc) == -1){
                        printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable inside array is not initialized\n",t_id, pc);
                        free_var_mem(h_loc);
                        kh_destroy(khStrInt, h_loc);
                         
                        return -1;
                    }     
                    k = kh_put(khStrInt, h_loc, arg1, &r);
                    if(r)
                        kh_key(h_loc, k) = strdup(arg1);
                }
                value = kh_get_val(khStrInt, h_loc, arg1, UNINITIALIZED);
            }else{
                value = atoi(arg1);
            }
            if(value == UNINITIALIZED){
                printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable not initialized\n",t_id, pc+1);
                free_var_mem(h_loc);
                kh_destroy(khStrInt, h_loc);
                 
                return -1;
            } 
            //arg2
            if(check_if_var(arg2)){
                if(check_if_array(arg2, h_loc) == TRUE){
                    if(getTempTokenArray(arg2, h_loc) == -1){
                        printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable inside array is not initialized\n",t_id, pc);
                        kh_destroy(khStrInt, h_loc);
                         
                        return -1;
                    }     
                    k = kh_put(khStrInt, h_loc, arg2, &r);
                    if(r)
                        kh_key(h_loc, k) = strdup(arg2);
                }
                value2 = kh_get_val(khStrInt, h_loc, arg2, UNINITIALIZED);
            }else{
                value2 = atoi(arg2);
            }
            if(value2 == UNINITIALIZED){
                printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable not initialized\n",t_id, pc+1);
                free_var_mem(h_loc);
                kh_destroy(khStrInt, h_loc);
                 
                return -1;
            } 
            //check lebel
            k = kh_get(khStrInt, h_loc, arg3);
            if(kh_exist(h_loc, k) == FALSE){
                printf("(Thread id: %d)  Wrong at run time. Line: (%d). Label doesn't exist\n",t_id, pc+1);
                free_var_mem(h_loc);
                kh_destroy(khStrInt, h_loc);
                 
                return -1; 
            }
            //compare
            if(value <= value2){
                pc = kh_get_val(khStrInt, h_loc, arg3, -1);
                if(pc==-1){
                    printf("(Thread id: %d)  Wrong at run time. Line: (%d). Error at label.\n",t_id, pc+1);
                    free_var_mem(h_loc);
                    kh_destroy(khStrInt, h_loc);
                     
                    return -1; 
                }
                continue;
            }
        }
        else if(strcmp(instruction, "BREQ") == 0){
            //arg1
            if(check_if_var(arg1)){
                if(check_if_array(arg1, h_loc) == TRUE){
                    if(getTempTokenArray(arg1, h_loc) == -1){
                        printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable inside array is not initialized\n",t_id, pc);
                        free_var_mem(h_loc);
                        kh_destroy(khStrInt, h_loc);
                         
                        return -1;
                    }     
                    k = kh_put(khStrInt, h_loc, arg1, &r);
                    if(r)
                        kh_key(h_loc, k) = strdup(arg1);
                }
                value = kh_get_val(khStrInt, h_loc, arg1, UNINITIALIZED);
            }else{
                value = atoi(arg1);
            }
            if(value == UNINITIALIZED){
                printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable not initialized\n",t_id, pc+1);
                free_var_mem(h_loc);
                kh_destroy(khStrInt, h_loc);
                 
                return -1;
            } 
            //arg2
            if(check_if_var(arg2)){
                if(check_if_array(arg2, h_loc) == TRUE){
                    if(getTempTokenArray(arg2, h_loc) == -1){
                        printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable inside array is not initialized\n",t_id, pc);
                        free_var_mem(h_loc);
                        kh_destroy(khStrInt, h_loc);
                         
                        return -1;
                    }     
                    k = kh_put(khStrInt, h_loc, arg2, &r);
                    if(r)
                        kh_key(h_loc, k) = strdup(arg2);
                }
                value2 = kh_get_val(khStrInt, h_loc, arg2, UNINITIALIZED);
            }else{
                value2 = atoi(arg2);
            }
            if(value2 == UNINITIALIZED){
                printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable not initialized\n",t_id, pc);
                free_var_mem(h_loc);
                kh_destroy(khStrInt, h_loc);
                 
                return -1;
            } 
            //check lebel
            k = kh_get(khStrInt, h_loc, arg3);
            if(kh_exist(h_loc, k) == FALSE){
                printf("(Thread id: %d)  Wrong at run time. Line: (%d). Label doesn't exist\n",t_id, pc+1);
                free_var_mem(h_loc);
                kh_destroy(khStrInt, h_loc);
                 
                return -1; 
            }
            //compare
            if(value == value2){
                pc = kh_get_val(khStrInt, h_loc, arg3, -1);
                if(pc==-1){
                    printf("(Thread id: %d)  Wrong at run time. Line: (%d). Error at label.\n",t_id, pc+1);
                    free_var_mem(h_loc);
                    kh_destroy(khStrInt, h_loc);
                     
                    return -1; 
                }
                continue;
            }
        }
        else if(strcmp(instruction, "BRA") == 0){
            k = kh_get(khStrInt, h_loc, arg1);
            if(kh_exist(h_loc, k) == FALSE){
                printf("(Thread id: %d)  Wrong at run time. Line: (%d). Label doesn't exist\n",t_id, pc+1);
                free_var_mem(h_loc);
                kh_destroy(khStrInt, h_loc);
                 
                return -1; 
            }
            pc = kh_get_val(khStrInt, h_loc, arg1, -1);
            if(pc==-1){
                    printf("(Thread id: %d)  Wrong at run time. Line: (%d). Error at label.\n",t_id, pc+1);
                    free_var_mem(h_loc);
                    kh_destroy(khStrInt, h_loc);
                     
                    return -1; 
                }
            continue;
        }
        else if(strcmp(instruction, "DOWN") == 0){
          
            value = kh_get_val(khStrInt, h_glob, arg1, -1);
            if(value == UNINITIALIZED)
                return(1);
            if(value == 0)
                pc--;
            else
                kh_set(khStrInt, h_glob, arg1, value-1);    
        }
        else if(strcmp(instruction, "UP") == 0){
             
            value = kh_get_val(khStrInt, h_glob, arg1, -1);
            if(value == UNINITIALIZED)
                return(1);
            kh_set(khStrInt, h_glob, arg1, value+1);    
            
        }
        else if(strcmp(instruction, "SLEEP") == 0){
            if(check_if_var(arg1)){
                if(check_if_array(arg1, h_loc) == TRUE){
                    if(getTempTokenArray(arg1, h_loc) == -1){
                        printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable inside array is not initialized\n",t_id, pc);
                        free_var_mem(h_loc);
                        kh_destroy(khStrInt, h_loc);
                         
                        return -1;
                    }     
                    k = kh_put(khStrInt, h_loc, arg1, &r);
                    if(r)
                        kh_key(h_loc, k) = strdup(arg1);
                }
                value = kh_get_val(khStrInt, h_loc, arg1, UNINITIALIZED);
            }else{
                value = atoi(arg1);
            }
            if(value == UNINITIALIZED){
                printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable not initialized\n",t_id, pc+1);
                free_var_mem(h_loc);
                kh_destroy(khStrInt, h_loc);
                 
                return -1;
            } 
            
            curr->secOfSleep = value;
            curr->startSleepTime = clock();
            pc++;
            curr->state = SLEEP;
            return(1);
            
        }
        else if(strcmp(instruction, "PRINT") == 0){
            printf("%s", token[pc][1]);
            for(j=2; token[pc][j][0] != '\0'; j++){
                strcpy(arg2, token[pc][j]);
                if(check_if_var(arg2)){
                    if(check_if_array(arg2, h_loc) == TRUE){
                        if(getTempTokenArray(arg2, h_loc) == -1){
                            printf("(Thread id: %d)  Wrong at run time. Line: (%d). Variable inside array is not initialized\n",t_id, pc);
                            free_var_mem(h_loc);
                            kh_destroy(khStrInt, h_loc);
                             
                            return -1;
                        }     
                        k = kh_put(khStrInt, h_loc, arg2, &r);
                        if(r)
                            kh_key(h_loc, k) = strdup(arg2);
                    }
                    value = kh_get_val(khStrInt, h_loc, arg2, UNINITIALIZED);
                }else{
                    value = atoi(token[pc][j]);
                }
                if(value == UNINITIALIZED){
                    printf("\n(Thread id: %d)  Wrong at run time. Line: (%d). Variable not initialized\n",t_id, pc+1);
                    free_var_mem(h_loc);
                    kh_destroy(khStrInt, h_loc);
                     
                    return -1;
                } 
                printf(" %d", value);
            }
            printf("\n");
        }
        else if(strcmp(instruction, "RETURN") == 0){
            break;
        }
        else if(strcmp(instruction, "#PROGRAM") == 0){
            
        }else{
            printf("Uknown instruction: %s\n", instruction);
            break;
        }
        
        
        
        
        pc++;
        counter++;
    }

    //debugging
    printf("size: %d\tbuckets: %d\n", kh_size(h_loc), kh_n_buckets(h_loc));
    printf("\nIterate all LOCAL keys\n");
    for (k = kh_begin(h_loc); k != kh_end(h_loc); ++k) {
        if (kh_exist(h_loc, k)) {
            const char *key = kh_key(h_loc,k);
            //kh_value(h_loc, k) = UNINITIALIZED;
            tval = kh_value(h_loc, k);
            printf("key=%s  val=%d\n", key, tval);
        }
    }
    
        free_var_mem(h_loc);
        kh_destroy(khStrInt, h_loc);
        return(0);
    }









void* thread_fun(){
    
    int delete_curr, retCompile, retRun;
    struct plist * curr = head->nxt;
    
    //loop th lista me ta programmata
    while(1){
        delete_curr = FALSE; //flag gia ean xreiastei na digrapsume to programma
        if(curr != head && curr != NULL){
            if(curr->state == STOPPED){ //an den exei teleiosei h trexei hdh
                if(curr->compiled == FALSE){ //ean exei kanei compile toy kanoume
                    retCompile = compile(curr);
                    if(retCompile == -1)  //uphrxe error kai 8a to diagrapsoume
                        delete_curr = TRUE;
                }
                else{
                    retRun = run(curr); //to trexoyme
                    if(retRun == -1){ //uphrxe error kai 8a to diagrapsoume
                        delete_curr = TRUE;
                    }else if(retRun == 0){
                        curr->state = FINISHED; //teleiwse
                    } 
                }
            }else if(curr->state == SLEEP){ //an koimatai kia prepei na ksipnisoume trexoume ton idio node
                if(check_if_should_wake(curr) == TRUE)
                    continue;
            }
            
            curr = curr->nxt; //next program in the list
            if(delete_curr)   //ean auto pou treksame eixe error to diagrafoume apo th lista
                list_remove(curr->prv->id);
        }
    }
    
    return NULL;
}


int main(int argc, char *argv[] ){
    
    char lineInput[MAX_LINE_LENGTH],
         line_tokens[MAX_WORDS_PER_LINE][MAX_WORD_LENGTH],
         thread_argv[MAX_WORDS_PER_LINE][MAX_WORD_LENGTH],
         thread_id_str[MAX_WORD_LENGTH],
         *temp_string;
    int i, j,
        thread_argc,
        thread_id = 0,
        num_threads = 0;
    pthread_t thread[40];
    
    /*** global hash table*******/
    h_glob = kh_init(khStrInt); 
    
    /******initialize memory *******/
    for(j=0; j < MAX_WORDS_PER_LINE; j++){
        memset(line_tokens[j], '\0', sizeof(line_tokens[j]));
        memset(thread_argv[j], '\0', sizeof(thread_argv[j]));
    }
    
    /****initialize list********/
    list_init();
    
    while(1){ //every new line instruction
        printf("\n\t~~~~~~~~~~~~~\nGive an instruction:\n\n");
        fgets(lineInput, MAX_LINE_LENGTH, stdin);
        
        i = 0; 
        temp_string = strtok(lineInput, s);  // get the first token 
        while( temp_string != NULL ) {  // walk through other tokens on the same line  
            strcpy(line_tokens[i], temp_string);
            //next token on the line
            i++;
            temp_string = strtok(NULL, s);
        }
        
        if(!strcmp(line_tokens[0], "run")){
            
            thread_argc = i;
            sprintf(thread_id_str, "%d", thread_id);
            strcpy(thread_argv[0], thread_id_str);
            for(j=1; j <= thread_argc; j++) 
                strcpy(thread_argv[j], line_tokens[j]);
            
            //insert in threads list
            list_insert(thread_id, thread_argc, thread_argv); //if wrong then delete
            
            while(num_threads < MAX_THREADS){
                //create thread
                pthread_create(&thread[num_threads], NULL, thread_fun, NULL);
                num_threads++;
            }
                        
            thread_id++;
        }
        else if(!strcmp(line_tokens[0], "list")){
            list_display();
        }
        else if(!strcmp(line_tokens[0], "kill")){
            if(list_remove(atoi(line_tokens[1])) == FALSE){
                printf("Couldn't remove program with id %s\n", line_tokens[1]);
            }
        }
        else if(!strcmp(line_tokens[0], "exit")){
            break;
        }else{
            printf("Wrong instruction. Gine one of the list:\n[run] [list] [kill] [exit]\n");
            continue;
        }
        
        
        
        
    }
    
    
    //free all list nodes
    for(i=0; i<=thread_id;i++){
        list_remove(i);
    }
    
    
    
    
    kh_destroy(khStrInt, h_glob);
    return(0);
}













