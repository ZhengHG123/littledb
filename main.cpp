#include <iostream>
#include <cstring>//#include <string.h>
#include <time.h>
#define COLUMN_USERNAME_SIZE 32
#define COLUMN_EMAIL_SIZE 255

typedef struct{
    char* buffer;
    size_t buffer_length;
    ssize_t input_length;
}InputBuffer;//input buffer
typedef enum{
    META_COMMAND_SUCCESS,
    META_COMMAND_UNRECONIZED_COMMAND
}MetaCommandResult;//Non-SQL statements like '.exit' are called "meta-commands".They are all start with a dot.
typedef enum{
    PREPARE_SUCCESS,
    PREPARE_STRING_TOO_LONG,
    PREPARE_NEGATIVE_ID,
    PREPARE_UNRECOGNIZED_STATEMENT,
    PREPARE_SYNTAX_ERROR
}PrepareResult;
typedef enum{
    STATEMENT_INSERT,STATEMENT_SELECT
}StatementType;
typedef enum{
    EXECUTE_SUCCESS,
    EXECUTE_TABLE_FULL
}ExecuteResult;
typedef struct{
    uint32_t id;
    char username[COLUMN_USERNAME_SIZE+1];
    char email[COLUMN_EMAIL_SIZE+1];
}Row;//This is where the table structure is defined.
typedef struct {
    StatementType type;//eg. STATEMENT_INSERT,STATEMENT_SELECT
    Row row_to_insert;//only used by insert statement
}Statement;
#define size_of_attribute(Struct,Attribute) sizeof(((Struct*)0)->Attribute)
const uint32_t ID_SIZE=size_of_attribute(Row,id);
const uint32_t USERNAME_SIZE=size_of_attribute(Row,username);
const uint32_t EMAIL_SIZE=size_of_attribute(Row,email);
const uint32_t ID_OFFSET=0;
const uint32_t USERNAME_OFFSET=ID_OFFSET+ID_SIZE;
const uint32_t EMAIL_OFFSET=USERNAME_OFFSET+USERNAME_SIZE;
const uint32_t ROW_SIZE=ID_SIZE+USERNAME_SIZE+EMAIL_SIZE;

void serialize_row(Row* source, char* destination){ //this compiler seems don't support arithmetic on void*, so I change void* to char*.Same as function named deserialize_row.
    memcpy(destination + ID_OFFSET, &(source->id),ID_SIZE);
    memcpy(destination + USERNAME_OFFSET, &(source->username),USERNAME_SIZE);
    memcpy(destination + EMAIL_OFFSET, &(source->email), EMAIL_SIZE);
}//row to mem??
void deserialize_row(char* source, Row* destination){
    memcpy(&(destination->id),source+ID_OFFSET,ID_SIZE);
    memcpy(&(destination->username),source+USERNAME_OFFSET,USERNAME_SIZE);
    memcpy(&(destination->email),source+EMAIL_OFFSET,EMAIL_SIZE);
}//mem to row??
const uint32_t PAGE_SIZE=4096;
#define TABLE_MAX_PAGES 100
const uint32_t ROWS_PER_PAGE=PAGE_SIZE/ROW_SIZE;
const uint32_t TABLE_MAX_ROWS=ROW_SIZE*TABLE_MAX_PAGES;
typedef struct {
    uint32_t num_rows; //how many rows in the table.
    char* pages[TABLE_MAX_PAGES];
}Table;
char* row_slot(Table* table, uint32_t row_num){//this is where we figure out where to read/write in memory for a particular row.
    uint32_t page_num=row_num/ROWS_PER_PAGE; //the page number of the row
    char* page=table->pages[page_num];
    if(page==NULL){
        //allocate memory only when we try to access page
        page=table->pages[page_num]=(char*)malloc(PAGE_SIZE);
    }
    uint32_t row_offset=row_num%ROWS_PER_PAGE;//row offset
    uint32_t byte_offset=row_offset*ROW_SIZE;//byte offset
    return (byte_offset+(char*)page);
}//return a pointer which points to the position we read/write a row.

InputBuffer* new_input_buffer(){
    auto* input_buffer=(InputBuffer*)malloc(sizeof(InputBuffer));
    input_buffer->buffer=nullptr;
    input_buffer->buffer_length=0;
    input_buffer->input_length=0;
    return input_buffer;
}//malloc input_buffer,return a pointer pointing at this input_buffer;
void print_prompt(){
    printf("db > ");
}//print a prompt word of input in terminal
void read_input(InputBuffer* input_buffer){
    ssize_t  bytes_read=getline(&(input_buffer->buffer),&(input_buffer->buffer_length),stdin);
    if(bytes_read<=0){
        printf("Error reading input\n");
        exit(EXIT_SUCCESS);//?????????????
    }
    input_buffer->input_length=bytes_read-1;
    input_buffer->buffer[bytes_read-1]=0;
}//use getline() to read the input into the buffer;
void close_input_buffer(InputBuffer* input_buffer){
    free(input_buffer->buffer);
    free(input_buffer);
}//free the memory allocate for an instance of InputBuffer* and the buffer element of the respectice structure;
MetaCommandResult do_meta_command(InputBuffer* input_buffer){
    if(strcmp(input_buffer->buffer,".exit")==0){
        printf("bye~");
        close_input_buffer(input_buffer);
        exit(EXIT_SUCCESS);
    }else{
        return META_COMMAND_UNRECONIZED_COMMAND;
    }
}//a wrapper for existing functionality that leaves room for more commands.
void print_row(Row* row){
    std::cout<<"||"<<row->id<<"|"<<row->username<<"|"<<row->email<<"||"<<std::endl;
}
PrepareResult prepare_insert(InputBuffer* inputBuffer,Statement* statement){
    statement->type=STATEMENT_INSERT;
    char* keyword=strtok(inputBuffer->buffer," ");
    char* id_string=strtok(NULL," ");
    char* username=strtok(NULL," ");
    char* email=strtok(NULL," ");
    if(id_string==NULL||username==NULL||email==NULL){
        return PREPARE_SYNTAX_ERROR;
    }
    int id=atoi(id_string);
    if (id<0){
        return PREPARE_NEGATIVE_ID;
    }
    if(strlen(username)>COLUMN_USERNAME_SIZE){
        return PREPARE_STRING_TOO_LONG;
    }
    if(strlen(email)>COLUMN_EMAIL_SIZE){
        return PREPARE_STRING_TOO_LONG;
    }
    statement->row_to_insert.id=id;
    strcpy(statement->row_to_insert.username,username);
    strcpy(statement->row_to_insert.email,email);
    return PREPARE_SUCCESS;
}
PrepareResult prepare_statement(InputBuffer* input_buffer,Statement* statement){
    if(strncmp(input_buffer->buffer,"insert",6)==0){
        return prepare_insert(input_buffer,statement);
    }
    if(strcmp(input_buffer->buffer,"select")==0){
        statement->type=STATEMENT_SELECT;
        return PREPARE_SUCCESS;
    }

    return PREPARE_UNRECOGNIZED_STATEMENT;
}//Note: we use for strncmp for "insert" since the "insert" keyword will be followed by data.

ExecuteResult execute_insert(Statement* statement, Table* table){
    if(table->num_rows>=TABLE_MAX_ROWS){
        return EXECUTE_TABLE_FULL;
    }
    Row* row_to_insert=&(statement->row_to_insert);
    serialize_row(row_to_insert, row_slot(table,table->num_rows));//the first param is the row we want to insert,the second param is the pointer which points to the address to insert.
    table->num_rows += 1;
    return EXECUTE_SUCCESS;
}
ExecuteResult execute_select(Statement* statement, Table* table){
    Row row;
    for(uint32_t i=0;i<table->num_rows;i++){
        deserialize_row(row_slot(table,i),&row);//row in mem to struct Row.
        print_row(&row);
    }
    return EXECUTE_SUCCESS;
}
ExecuteResult execute_statement(Statement* statement,Table* table){
    switch (statement->type) {
        case(STATEMENT_INSERT):
            return execute_insert(statement, table);
        case(STATEMENT_SELECT):
            return execute_select(statement,table);
    }
}//execute statement like insert ,select....
Table* new_table(){
    Table* table=(Table*)malloc(sizeof(Table));
    table->num_rows=0;
    for(uint32_t i=0;i<TABLE_MAX_PAGES;i++){
        table->pages[i]=NULL;
    }
    return table;
}//seems to initialize the table; malloc table and its pages
void free_table(Table* table){
    for(int i=0;table->pages[i];i++){
        free(table->pages[i]);
    }
    free(table);
}//free table's pages and the table
int main() {
    Table* table = new_table();//until now,this dbms doesn't support 'create table' statement, so we create a table here using func new_table() which malloc the table and its pages.
    InputBuffer* input_buffer=new_input_buffer();
    while(true){
        print_prompt();
        read_input(input_buffer);
        clock_t start,finish;
        start = clock();
        if(input_buffer->buffer[0]=='.'){
            switch(do_meta_command(input_buffer)) {
                case (META_COMMAND_SUCCESS):
                    continue;
                case (META_COMMAND_UNRECONIZED_COMMAND):
                    printf("Unrecognized command '%s'\n", input_buffer->buffer);
                    continue;
            }
        }

        Statement statement;
        switch(prepare_statement(input_buffer,&statement)){
            case (PREPARE_SUCCESS):
                break;
            case (PREPARE_NEGATIVE_ID):
                printf("ID must be positive.\n");
                continue;
            case (PREPARE_STRING_TOO_LONG):
                printf("String is too long.\n");
                continue;
            case (PREPARE_SYNTAX_ERROR):
                printf("Syntax error. Could not parse statement.\n");
                continue;
            case (PREPARE_UNRECOGNIZED_STATEMENT):
                printf("Unrecognized keyword at start of '%s'.\n",input_buffer->buffer);
                continue;
        }

        //execute_statement(&statement);
        switch (execute_statement(&statement,table)) {
            case (EXECUTE_SUCCESS):
                printf("Executed.\n");
                break;
            case (EXECUTE_TABLE_FULL):
                printf("Error:Table full.\n");
                break;
        }
        finish=clock();
        double duration=(double)(finish-start)/CLOCKS_PER_SEC;
        printf("(%f sec)\n", duration);
    }
    return 0;
}
