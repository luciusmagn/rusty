#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <winstring.h>
#include "dirent.h"
#else
#include <dirent.h>
#include <unistd.h>
#include <dirent.h>
#endif

#include "util.h"
#include "mpc.h"
#include "arg.h"

//macros
#define error(x)     do { _error(x, __FILE__, __LINE__); } while(0)
#define emkdir(x,y)  do { if(mkdir(x,y) && access(x, F_OK)) {      \
                            char* msg; asprintf(&msg,"unable to create directory \"%s\"", x);\
                            builderror(msg); free(msg);\
                            }} while(0)
#define builderror(msg)   do { printf(ANSI_RED "build error" ANSI_RESET ": \"%s\"\n", msg); } while(0)

#define parser       mpc_parser_t*
#define entry        struct dirent*
#define MAX_DEPTH    10
#define ANSI_RED     "\x1b[31m"
#define ANSI_GREEN   "\x1b[32m"
#define ANSI_YELLOW  "\x1b[33m"
#define ANSI_BLUE    "\x1b[34m"
#define ANSI_MAGENTA "\x1b[35m"
#define ANSI_CYAN    "\x1b[36m"
#define ANSI_RESET   "\x1b[0m"

#ifdef _WIN32
#define PATH_C '\\'
#define R_OK _A_NORMAL
#else
#define PATH_C '/'
#endif


#ifdef _WIN32
#define OS "windows"
#elif __APPLE__
#define OS "osx"
#elif __linux__
#define OS "linux"
#elif __unix__
#define OS "unix"
#else
#define OS "other"
#endif

//types
typedef enum
{
    EXECUTABLE,
    LIBSHARED,
    LIBSTATIC,
    OBJECT
} target_type;
typedef struct _llist
{
    void* data;
    struct _llist* n;
} llist;
typedef struct
{
    char* name;
    llist* depends;
} file;
typedef struct
{
    int8 built;
    char* ident;
    char* name;
    char* output;
    char* sourcedir;
    target_type type;
    llist* run;
    llist* files;
    llist* depends;
    llist* flags;
    llist* install;
    llist* uninstall;
    llist* link;
} target;
typedef struct
{
    int8 fullrebuild;
    int8 printast;
    int8 printinfo;
    int8 verbose;
    int8 install;
    int8 uninstall;
    int8 time;
    int8 wanted_only; //print and show info only about wanted targets
    int8 only_check; //don't build the targets, just check the ast, process arguments and exit
} options;

//functions
llist* llist_new(void*);
int32 llist_total(llist*, int32);
void llist_put(llist*, void*);
void* llist_get(llist*, int32, int32);
void llist_free(llist*);

char* get_string(mpc_ast_t*);
char* adler32(const char*, uint64);
char* readfile(const char*);
char* filename(char*);
int8 modified(char*);
int32 asprintf(char**,const char*,...);
int32 vasprintf(char**,const char*,va_list);
int32 search(llist*, char*);
int32 searchstr(llist*,char*);
void _error(char*, char*, int32);
void deletedir(char*);
void deleteast(mpc_ast_t*);

void parse();

void read_ast(mpc_ast_t*);
void read_trg(mpc_ast_t*);
void read_if(mpc_ast_t*);
void read_list(mpc_ast_t*,llist**);
void read_type(mpc_ast_t*, target*);
void read_dir(target*,char*);

void process(llist*);
void builder(llist*);
void linker(llist*);
void install(llist*);

void cleanup();
void handleopts(llist*);
int8 option(llist**, char**, int*);
int8 checkdepends(llist*);
void printhelp();
void printabout();



//global vars
options* opts;
char* compiler = NULL;
char* output_path;
mpc_ast_t* tree;
llist* targets = NULL;


/*
** Functions
*/

//linked list implementation
llist* llist_new(void* first)
{
    if(first == NULL) return NULL;
    llist* l = malloc(sizeof(llist));
    l->data = first;
    l->n = NULL;
    return l;
}
void llist_put(llist* l, void* data)
{
    if(!l) return;
    if(l && l->n == NULL)
    {
        l->n = malloc(sizeof(llist));
        l->n->data = data;
        l->n->n = NULL;
    }
    else llist_put(l->n, data);
}
int32 llist_total(llist* l, int32 carry)
{
    if(!l) return 0;
    if(l->n != NULL) return llist_total(l->n, carry + 1);
    else return carry + 1;
}
void* llist_get(llist* l, int32 index, int32 carry)
{
    if(!l) return NULL;
    if(carry > index) return NULL;
    if(carry < index && l->n) return llist_get(l->n, index, carry + 1);
    if(carry == index && l->data != NULL) return l->data;
    return NULL;
}
void llist_free(llist* l)
{
	if(!l) return;
	else free(l->n);
	free(l);
}

//utils
char* get_string(mpc_ast_t* ast)
{
    return ast->children[2]->children[1]->contents;
}

char* adler32(const char* str, uint64 len)
{
  uint32 a = 1; uint32 b = 0;
  for(uint64 i=0; i<len; i++)
  {
      a = (a + str[i]) % 65521;
      b = (a + b) % 65521;
  }
  uint32 dec = (b << 16) | a;
  char* hex = malloc(sizeof(char) * 10);
  snprintf(hex, 10, "%x", dec);
  return hex;
}

char* readfile(const char* filename)
{
    FILE* file;
    int32 lsize;
    char* result;
    file = fopen(filename, "rb");
    if (!file)
    {
        perror(filename);
        exit(1);
    }
    fseek(file, 0L, SEEK_END);
    lsize = ftell(file);
    rewind(file);

    result = calloc(1, lsize + 1);
    if (!result)
    {
        fclose(file);
        error("failed to alloc memory");
        exit(1);
    }
    if (fread(result, lsize, 1, file) != 1)
    {
        fclose(file);
        free(result);
        error("failed to read file");
        exit(1);
    }
    fclose(file);
    return result;
}

char* filename(char* path)
{
    if (strchr(path, PATH_C))
        return strchr(path, PATH_C) + 1;
    else return path;
}

int8 modified(char* name)
{
    if(opts->fullrebuild) return 1;
    char* file = readfile(name);
    char* checksum_new =
        adler32(file, strlen(file));
    emkdir(".rusty", ALLPERMS);
    char* sumname;
    asprintf(&sumname, ".rusty/%s.sum", filename(name));
    int8 res = 0;
    if(access(sumname, R_OK) == 0)
    {
    	char* checksum_old = readfile(sumname);
    	if(strcmp(checksum_old, checksum_new) != 0) res = 1;
    	else
    	{
        	remove(sumname);
       		FILE* sumfile = fopen(sumname, "w+");
        	fputs(checksum_new, sumfile);
    	    fflush(sumfile);
	        fclose(sumfile);
            free(checksum_old);
    	}
    }
    else res = 1;

    free(sumname);
    free(checksum_new);
    return res;
}

#ifndef HAVE_ASPRINTF
int32 asprintf(char **str, const char *fmt, ...)
{
    int size = 0;
    va_list args;
    va_start(args, fmt);
    size = vasprintf(str, fmt, args);
    va_end(args);
    return size;
}

int32 vasprintf(char **str, const char *fmt, va_list args)
{
    int32 size = 0;
    va_list tmpa;
    va_copy(tmpa, args);
    size = vsnprintf(NULL, size, fmt, tmpa);
    va_end(tmpa);
    if (size < 0)
        return -1;
    *str = (char *)malloc(size + 1);
    if (NULL == *str)
        return -1;
    size = vsprintf(*str, fmt, args);
    return size;
}
#endif

int32 search(llist* l, char* ident)
{
    if(l == NULL) return 0;
    if(l->n == NULL && strcmp(((target*)(l->data))->ident,ident) != 0) return 0;
    else if(strcmp(((target*)(l->data))->ident, ident) == 0) return 1;
    else if(l->n == NULL) return 0;
    return search(l->n, ident);
}

int32 searchstr(llist* l, char* ident)
{
    int32 flag = 0;
    for(int32 x = 0; x < llist_total(l, 0); x++)
        if(strcmp(ident, llist_get(l, x, 0)) == 0) { flag = 1; break; }
    return flag;
}

void _error(char* msg, char* file, int32 line)
{
    printf(ANSI_RED "error" ANSI_RESET ": \"%s\"" ANSI_YELLOW " at %s:%d\n" ANSI_RESET, msg, file, line);
    exit(-1);
}

void deletedir(char* name)
{
    DIR* dir = opendir(name);
    if(!dir) return;
    entry ent;
    while( (ent = readdir(dir)) )
    {
        char* path;
        asprintf(&path, "%s/%s", name, ent->d_name);
        //have to exclude . and .. and check for read permission
        if(access(path, R_OK) == 0 && strcmp(ent->d_name, "..")
                && strcmp(ent->d_name, "."))
        {
            if(ent->d_type == DT_DIR)
                deletedir(path);
            else remove(path);
            free(path);
        }
    }
    rmdir(name);
}

void deleteast(mpc_ast_t* a)
{
	for (int32 i = 0; i < a->children_num; i++)
		deleteast(a->children[i]);
	free(a);
}


/*****************************
 *           MAIN            *
 *****************************/
int32 main(int32 argc, char* argv[])
{
    llist* wanted = NULL;
    clock_t begin = clock();
    opts = calloc(1, sizeof(options));
    if(argc == 1) wanted = llist_new("all");

    ARGBEGIN
    {
        FLAG('h', printhelp())
        FLAG('d', chdir((++argv)[0]))
        FLAG('r', opts->fullrebuild = 1)
        FLAG('i', opts->printinfo = 1)
        FLAG('a', opts->printast = 1)
        FLAG('o', output_path = (++argv)[0])
        FLAG('c', compiler = (++argv)[0])
        FLAG('t', opts->time = 1)
        FLAG('v', opts->verbose = 1)
        FLAG('w', opts->wanted_only = 1)
        FLAG('n', opts->only_check = 1)
        default:
            printf("unrecognized option: %c\n", ARGC());
            break;
    }
    ARGEND

    int32 i = 10;
    while(access("rusty.txt", R_OK) != 0)
    {
        chdir("..");
        if(i-- < 1) error("rusty file not found");
    }
    parse();
    if(!wanted) wanted = llist_new("all");
    process(wanted);
    clock_t end = clock();
    if(opts->time) printf("execution time: %f seconds\n", (double)(end - begin) / CLOCKS_PER_SEC);
	free(opts);
	llist_free(wanted);
    return 0;
}

void parse()
{
    mpc_result_t r;

    parser ident = mpc_new("ident");
    parser string = mpc_new("string");
    parser name = mpc_new("name");
    parser type = mpc_new("type");
    parser flags = mpc_new("flags");
    parser install = mpc_new("install");
    parser uninstall = mpc_new("uninstall");
    parser attribute = mpc_new("attribute");
    parser file = mpc_new("file");
    parser run = mpc_new("run");
    parser depends = mpc_new("depends");
    parser link = mpc_new("link");
    parser buildtrg = mpc_new("buildtrg");
    parser dir = mpc_new("dir");
    parser output = mpc_new("output");
    parser sourcedir = mpc_new("sourcedir");
    parser target = mpc_new("target");
    parser build = mpc_new("build");
    parser os = mpc_new("os");
    parser system = mpc_new("system");
    parser compiler = mpc_new("compiler");
    parser rusty = mpc_new("rusty");

    mpca_lang(MPCA_LANG_DEFAULT,
              "ident    : /[a-zA-Z0-9_]+/ ;                                                          \n"
              "string   : '\"' /([^\\\\\"]|\\\\.)*/ '\"' ;                                           \n"
              "name     : \"name\" ':' <string> ';' ;                                                \n"
              "type     : \"type\" ':' (\"executable\"|\"libshared\"|\"libstatic\"|\"object\") ';' ; \n"
              "flags    : \"flags\" ':' '{' <string> (',' <string>)* ','? '}' ';' ;                  \n"
              "install  : \"install\" ':' '{' <string> (',' <string>)* ','? '}' ';' ;                \n"
              "uninstall: \"uninstall\" ':' '{' <string> (',' <string>)* ','? '}' ';' ;              \n"
              "attribute: '@' \"depends\" '(' <string> (',' <string>)* ')' ;                         \n"
              "file     : \"file\" ':' <string> <attribute>? ';' ;                                   \n"
              "run      : \"run\" ':' '{' <string> (',' <string>)* ','? '}' ';' ;                    \n"
              "depends  : \"depends\" ':' <string> ';' ;                                             \n"
              "link     : \"link\" ':' <ident> ';' ;                                                 \n"
              "buildtrg : \"build\" ':' <ident> ';' ;                                                \n"
              "dir      : \"dir\" ':' <string> ';' ;                                                 \n"
              "output   : \"output\" ':' <string> ';' ;                                              \n"
              "sourcedir: \"sourcedir\" ':' <string> ';' ;                                           \n"
              "target   : \"target\" <ident> ':' <name> <type>? <flags>? <output>?                   \n"
              "         (<run>|<sourcedir>|<file>|<dir>|<depends>|<install>|<uninstall>|<link>)+ ;   \n"
              "build    : \"build\" <ident> ':' <name> <type>? (<buildtrg>)+ ';' ;                   \n"
              "os       : (\"windows\" | \"osx\" | \"linux\" | \"unix\" | \"other\") ;               \n"
              "system   : \"if\" '(' <os> ')' '{' (<target>)+ '}' ;                                  \n"
              "compiler : \"compiler\" ':' <string> ';' ;                                            \n"
              "rusty    : /^/ <compiler> (<target> | <system>)+ /$/ ;                                \n"
              , ident, string, name, type, flags, install, uninstall, attribute, file, run, depends, link, buildtrg, dir,
                output, sourcedir, target, build, os, system, compiler, rusty, NULL);
    char* raw = readfile("rusty.txt");
    char* commentless = calloc(sizeof(char) * strlen(raw), 1);
    for (int32 i = 0, k = 0; i < strlen(raw); i++, k++)
    {
        if(raw[i] == '#') { while(raw[i] != '\n') i++; }
        commentless[k] = raw[i];
    }
    free(raw);
    if(mpc_parse("rusty.txt", commentless, rusty, &r))
    {
        tree = r.output;
        read_ast(r.output);
    }
    else
    {
        mpc_err_print(r.error);
        mpc_err_delete(r.error);
        exit(-1);
    }
    free(commentless);
    mpc_cleanup(21, ident, string, name, type, flags, install, uninstall, attribute, file, run, depends, link, buildtrg, dir, output,
                    sourcedir, target, build, os, system, compiler, rusty);
}

void read_ast(mpc_ast_t* ast)
{
    if(!compiler) compiler = get_string(ast->children[1]); //in case it's set by the -c/--compiler flag
    if(!compiler) error("couldn't read compiler name");
    for(int32 i = 0; i < ast->children_num; i++)
    {
        if(strcmp(ast->children[i]->tag, "target|>") == 0) read_trg(ast->children[i]);
        if(strcmp(ast->children[i]->tag, "system|>") == 0) read_if(ast->children[i]);
    }

    deleteast(ast);
}

void read_trg(mpc_ast_t* ast)
{
    target* trg = calloc(sizeof(target), 1);
    trg->ident = ast->children[1]->contents;
    for (int32 i = 0; i < ast->children_num; i++)
    {
        if(strcmp(ast->children[i]->tag, "name|>") == 0) trg->name = get_string(ast->children[i]);
        if(strcmp(ast->children[i]->tag, "output|>") == 0) trg->output = get_string(ast->children[i]);
        if(strcmp(ast->children[i]->tag, "sourcedir|>") == 0) trg->sourcedir = get_string(ast->children[i]);
        if(strcmp(ast->children[i]->tag, "file|>") == 0)
        {
            file* f = calloc(sizeof(file), 1);
            if(trg->sourcedir)
            {
                f->name = (char*)calloc(strlen(trg->sourcedir) + strlen(get_string(ast->children[i])) + 1, 1);
                strcat(f->name, trg->sourcedir);
                strcat(f->name, "/");
                strcat(f->name, get_string(ast->children[i]));
            }
            else
                f->name = get_string(ast->children[i]);
            if(ast->children[i]->children[3])
            for(int j = 3; j < ast->children[i]->children[3]->children_num; j++)
            {
                if(strcmp(ast->children[i]->children[3]->children[j]->tag, "string|>") == 0)
                {
                    if(!trg->sourcedir)
                    {
                        if(f->depends)
                            llist_put(f->depends, ast->children[i]->children[3]->children[j]->children[1]->contents);
                        else
                            f->depends = llist_new(ast->children[i]->children[3]->children[j]->children[1]->contents);
                    }
                    else
                    {
                        char* tmp = calloc(strlen(ast->children[i]->children[3]->children[j]->children[1]->contents)
                                           + strlen(trg->sourcedir)
                                           + 1, 1);
                        strcat(tmp, trg->sourcedir);
                        strcat(tmp, "/");
                        strcat(tmp, ast->children[i]->children[3]->children[j]->children[1]->contents);
                        if(f->depends)
                            llist_put(f->depends, tmp);
                        else
                            f->depends = llist_new(tmp);
                    }
                }
            }

            if(trg->files) llist_put(trg->files, f);
            else trg->files = llist_new(f);
        }
        if(strcmp(ast->children[i]->tag, "depends|>") == 0)
        {
            char* tmp;
            if(trg->sourcedir)
            {
                tmp = (char*)calloc(strlen(trg->sourcedir) + strlen(get_string(ast->children[i])) + 1, 1);
                strcat(tmp, trg->sourcedir);
                strcat(tmp, "/");
                strcat(tmp, get_string(ast->children[i]));
            }
            else
                tmp = get_string(ast->children[i]);
            if(trg->depends) llist_put(trg->depends, tmp);
            else trg->depends = llist_new(tmp);
        }
        if(strcmp(ast->children[i]->tag, "link|>") == 0)
        {
            if(trg->link) llist_put(trg->link, ast->children[i]->children[2]->contents);
            else trg->link = llist_new(ast->children[i]->children[2]->contents);
        }
        if(strcmp(ast->children[i]->tag, "dir|>") == 0) read_dir(trg, get_string(ast->children[i]));
        if(strcmp(ast->children[i]->tag, "run|>") == 0) read_list(ast->children[i], &trg->run);
        if(strcmp(ast->children[i]->tag, "flags|>") == 0) read_list(ast->children[i], &trg->flags);
        if(strcmp(ast->children[i]->tag, "install|>") == 0) read_list(ast->children[i], &trg->install);
        if(strcmp(ast->children[i]->tag, "uninstall|>") == 0) read_list(ast->children[i], &trg->uninstall);
        if(strcmp(ast->children[i]->tag, "type|>") == 0) read_type(ast->children[i], trg);
    }

    if(!targets) targets = llist_new(trg);
    else llist_put(targets, trg);

    for(int32 j = 0; j < llist_total(trg->run, 0); j++)
    {
        if(opts->verbose) printf(ANSI_BLUE "exec:" ANSI_GREEN "%s" ANSI_RESET "\n", (char*)llist_get(trg->run, j, 0));
        system((const char*)llist_get(trg->run, j, 0));
    }
}

void read_if(mpc_ast_t* ast)
{
    if(strcmp(ast->children[2]->contents, OS) != 0) return;
    for(int32 i = 0; i < ast->children_num; i++)
        if(strcmp(ast->children[i]->tag, "target|>") == 0) read_trg(ast->children[i]);
}

void read_list(mpc_ast_t* ast,llist** list)
{
    for(int32 i = 0; i < ast->children_num; i++)
    {
        if(strcmp(ast->children[i]->tag, "string|>") == 0)
        {
            if(*list) llist_put(*list, ast->children[i]->children[1]->contents);
            else *list = llist_new(ast->children[i]->children[1]->contents);
        }
    }
}

void read_type(mpc_ast_t* ast, target* trg)
{
    if(strcmp(ast->children[2]->contents, "executable") == 0) trg->type = EXECUTABLE;
    else if(strcmp(ast->children[2]->contents, "libshared") == 0)  trg->type = LIBSHARED;
    else if(strcmp(ast->children[2]->contents, "libstatic") == 0)  trg->type = LIBSTATIC;
    else if(strcmp(ast->children[2]->contents, "object") == 0) trg->type = OBJECT;
}

void read_dir(target* trg, char* name)
{
    DIR* dir = opendir(name);
    if(!dir)
    {
        char* msg;
        asprintf(&msg, "directory %s does not exist or cannot be opened", name);
        error(msg);
    }
    entry ent;
    while( (ent = readdir(dir)) )
    {
        char* path;
        asprintf(&path, "%s/%s", name, ent->d_name);
        //have to exclude . and .. and check for read permission
        if(access(path, R_OK) == 0 && strcmp(ent->d_name, "..")
                && strcmp(ent->d_name, ".")
                && (    strstr(ent->d_name, ".c")
                      ||strstr(ent->d_name, ".m")
                      ||strstr(ent->d_name, ".mm")
                      ||strstr(ent->d_name, ".M")
                      ||strstr(ent->d_name, ".go")
                      ||strstr(ent->d_name, ".cpp")
                      ||strstr(ent->d_name, ".d")
                      ||strstr(ent->d_name, ".s")
                      ||strstr(ent->d_name, ".cc")
                      ||strstr(ent->d_name, ".cp")
                      ||strstr(ent->d_name, ".cxx")
                      ||strstr(ent->d_name, ".c++")
                      ||strstr(ent->d_name, ".C")
                      ||strstr(ent->d_name, ".CPP")
                   )
          )
        {
            file* f = calloc(sizeof(file), 1);
            f->name = path;
            llist_put(trg->files, f);
        }
    }
    closedir(dir);
    return;
}

void process(llist* wanted)
{
    handleopts(wanted);
    if(!opts->uninstall && !opts->only_check)
    {
        builder(wanted);
        linker(wanted);
    }
    if(opts->install && opts->uninstall)
        puts(ANSI_BLUE "I don't know about you, but I think that uninstalling and installing at once is a bad idea. "
                       "But well, who am I to judge." ANSI_RESET);
    install(wanted);
    return;
}

void builder(llist* buildtargets)
{
    //check if all targets are found
    int32 errors = 0;
    for(int32 i = 0; i < llist_total(buildtargets, 0); i++)
    {
        search(targets, llist_get(buildtargets,i,0));
        if(!search(targets, llist_get(buildtargets, i, 0)) && strcmp(llist_get(buildtargets, i, 0), "all"))
        {
            errors++;
            char* msg;
            asprintf(&msg, "target %s not found", llist_get(buildtargets, i, 0));
            builderror(msg);
            free(msg);
        }
    }
    if(errors)
        error("one or more targets could not be found, aborting");
    //check if all files in given targets are present
    errors = 0;
    for(int32 i = 0; i < llist_total(targets, 0); i++)
    {
        target* current = llist_get(targets, i, 0);
        if(!searchstr(buildtargets, current->ident) && !searchstr(buildtargets, "all")) continue;
        for(int32 j = 0; j < llist_total(current->files, 0); j++)
        {
            if(access( ((file*)llist_get(current->files, j, 0))->name, R_OK) == 0) continue;
            errors++;
            char* msg;
            asprintf(&msg, "file %s does not exist or cannot be read", ((file*)llist_get(current->files, j, 0))->name);
            builderror(msg);
        }
        for(int32 j = 0; j < llist_total(current->depends, 0); j++)
        {
            if(access(llist_get(current->depends, j, 0), R_OK) == 0) continue;
            errors++;
            char* msg;
            asprintf(&msg, "file %s does not exist or cannot be read", llist_get(current->depends, j, 0));
            builderror(msg);
            free(msg);
        }
    }
    if(errors) error("one or more files were not accessible, aborting");
    //step 3 - compile each file to an object file
    errors = 0;
    emkdir("object", ALLPERMS);
    for(int32 i = 0; i < llist_total(targets, 0); i++)
    {
        target* current = llist_get(targets, i, 0);
        if(!searchstr(buildtargets, current->ident) && !searchstr(buildtargets, "all")) continue;
        printf(ANSI_MAGENTA "building target: " ANSI_YELLOW "%s" ANSI_RESET "\n", current->ident);
        char* dir;
        asprintf(&dir, "object/%s", current->ident);
        emkdir(dir, ALLPERMS);

        int8 depends_okay = 1;
        for(int32 j = 0; j < llist_total(current->depends, 0); j++)
            if(modified(llist_get(current->depends, j, 0))) { if(opts->verbose) puts(ANSI_BLUE "dependency file modified, rebuilding...");
                depends_okay = 0; break; }
        for(int32 j = 0; j < llist_total(current->files, 0); j++)
        {
            char* path;
            asprintf(&path, "object/%s/%s.o",
                     current->ident,
                     filename( ((file*)llist_get(current->files, j, 0))->name) );
            if(!access(path, R_OK) && !modified( ((file*)llist_get(current->files, j, 0))->name )
            && !checkdepends( ((file*)llist_get(current->files, j, 0))->depends) && depends_okay)
                continue;
            if(opts->verbose && depends_okay)
                printf((searchstr(buildtargets, "all") ? ANSI_BLUE "recompiling file %s...\n" : ANSI_BLUE "source file %s modified, recompiling...\n"), ((file*)llist_get(current->files, j, 0))->name);
            printf(ANSI_GREEN "\tbuilding file" ANSI_YELLOW " %s" ANSI_RESET "\n" , ((file*)llist_get(current->files, j, 0))->name );
            char* cmd;
            char* flags = " ";
            for(int32 j = 0; j < llist_total(current->flags, 0); j++)
                asprintf(&flags, "%s %s", flags, llist_get(current->flags, j, 0));
            asprintf(&cmd, "%s %s %s -c -o object/%s/%s.o",
                     compiler,
                     ((file*)llist_get(current->files, j, 0))->name,
                     flags,
                     current->ident,
                     filename( ((file*)llist_get(current->files, j, 0))->name ));
            if(opts->verbose) printf(ANSI_BLUE "exec:" ANSI_GREEN "%s" ANSI_RESET "\n", cmd);
            if(system(cmd))
            {
                errors++;
                printf(ANSI_RED "\tfailed to build file " ANSI_YELLOW "%s" ANSI_RESET "\n", ((file*)llist_get(current->files, j, 0))->name);
            }
            free(cmd);
            free(path);
            free(flags);
        }
        free(dir);
        if(errors)
            printf(ANSI_RED "failed to build target " ANSI_YELLOW "%s" ANSI_RESET "\n" , current->ident);
        current->built = 1;
    }
    if(errors) error("failed to build one or more targets, aborting");
}

void linker(llist* linktargets)
{
    int32 errors = 0;
    for(int32 i = 0; i < llist_total(targets, 0); i++)
    {
        target* current = llist_get(targets, i, 0);
        if(!llist_total(current->files, 0)) continue;
        mkdir((current->output ? current->output : "output"), ALLPERMS);
        if(!searchstr(linktargets, current->ident) && !searchstr(linktargets, "all")) continue;
        printf(ANSI_MAGENTA "linking target" ANSI_YELLOW " %s" ANSI_RESET "\n", current->ident);
        char* dir;
        asprintf(&dir, "output/%s", current->ident);
        if(!current->output)emkdir(dir, ALLPERMS);
        char* list = " ";
        for(int32 j = 0; j < llist_total(current->files, 0); j++)
            asprintf(&list, "%s object/%s/%s.o ", list, current->ident, filename( ((file*)llist_get(current->files, j, 0))->name ));
        for(int32 j = 0; j < llist_total(current->link, 0); j++)
        {
            char* name = llist_get(current->link, j, 0);
            target* trg;
            int8 found = 0;
            for(int32 k = 0; k < llist_total(targets, 0); k++)
                if(strcmp(((target*)llist_get(targets, k, 0))->ident, name) == 0)
                    { found = 1; trg = (target*)llist_get(targets, k, 0); break; }
            if(!found)
            {
                errors++;
                printf(ANSI_RED "target not found:" ANSI_YELLOW "%s" ANSI_RESET "\n", name);
                printf(ANSI_RED "unable to link a non-existing target\n" ANSI_RESET);
                break;
            }
            if(!trg->built)
            {
                printf("target \"%s\" not built yet, building...", trg->ident);
                builder(llist_new(trg->ident));
            }
            for(int32 k = 0; k < llist_total(trg->files, 0); k++)
                asprintf(&list, "%s object/%s/%s.o ", list, trg->ident, filename( ((file*)llist_get(trg->files, k, 0))->name ));
            printf(ANSI_GREEN "\tlinked with: " ANSI_YELLOW "%s" ANSI_RESET "\n", name);
        }
        char* flags = " ";
        for(int32 j = 0; j < llist_total(current->flags, 0); j++)
            asprintf(&flags, "%s %s", flags, llist_get(current->flags, j, 0));
        char* cmd;
        char* path = (output_path ? output_path : "output");
        if(current->output) path = current->output;
        switch(current->type)
        {
        case EXECUTABLE:
            if(current->output) asprintf(&cmd, "%s %s %s -o %s/%s", compiler, list, flags, path, current->name);
            else asprintf(&cmd, "%s %s %s -o %s/%s/%s", compiler, list, flags, path, current->ident, current->name);
            break;
        case LIBSHARED:
            if(current->output) asprintf(&cmd, "%s %s %s -shared -o %s/%s.so", compiler, list, flags, path, current->name);
            else asprintf(&cmd, "%s %s %s -shared -o %s/%s/%s", compiler, list, flags, path, current->ident, current->name);
            break;
        case LIBSTATIC:; //empty statement to convince C that this is not a label in front of a declaraton
            char cwd[256];
            char *list2 = " ";
            getcwd(cwd, 256);
            for(int32 j = 0; j < llist_total(current->files, 0); j++)
                asprintf(&list2, "%s %s/object/%s/%s.o ",
                         list2, cwd, current->ident, filename( ((file*)llist_get(current->files, j, 0))->name ));
            if (current->output)
            {
                chdir(path);
                asprintf(&cmd, "ar -rcs %s.a %s", current->name, list2);
            }
            else
            {
                char* path;
                asprintf(&path, "output/%s", current->ident);
                chdir(path);
                asprintf(&cmd, "ar -rcs %s.a %s", current->name, list2);
            }
            if(opts->verbose) printf(ANSI_BLUE "exec:" ANSI_GREEN "%s" ANSI_RESET "\n", cmd);
            if(system(cmd))
            {
                errors++;
                printf(ANSI_RED "failed to link target " ANSI_YELLOW "%s" ANSI_RESET "\n", current->ident);
            }
            chdir(cwd);
            free(list2);
            break;
        case OBJECT:; //empty statement to convince C that this is not a label in front of a declaraton
            char* path; asprintf(&path, "object/%s", current->ident);
            DIR* dir = opendir(path);
            entry ent;
            while( (ent = readdir(dir)) )
            {
                if( !(strcmp(ent->d_name, "..") && strcmp(ent->d_name, ".")) ) continue;
                FILE *src, *dest;
                char *src_path, *dest_path, *output_dir;
                asprintf(&src_path, "object/%s/%s", current->ident, ent->d_name);
                asprintf(&output_dir, "output/%s", current->ident);
                emkdir(output_dir, ALLPERMS);

                if(current->output)
                    asprintf(&dest_path, "%s/%s", current->output, ent->d_name);
                else
                    asprintf(&dest_path, "%s/%s/%s", (output_path ? output_path : "output"), current->ident, ent->d_name);

                if( !(src = fopen(src_path, "rb")) ) { errors++; printf(ANSI_RED "can't read " ANSI_YELLOW "%s" ANSI_RESET "\n", src_path); }
                if( !(dest = fopen(dest_path, "wb")) ) { errors++; printf(ANSI_RED "can't read " ANSI_YELLOW "%s" ANSI_RESET "\n", dest_path); }

                //courtesy of pmg
                size_t n, m;
                unsigned char buff[8192];
                do
                {
                    n = fread(buff, 1, sizeof(buff), src);
                    if (n) m = fwrite(buff, 1, n, dest);
                    else   m = 0;
                } while ((n > 0) && (n == m));
                if (m) printf(ANSI_RED "error copying file" ANSI_RESET "\n");
                fclose(src);
                fclose(dest);
                free(src_path);
                free(dest_path);
                free(output_dir);
            }
            closedir(dir);
            free(ent);
            free(path);
            break;
        }
        if(current->type == LIBSTATIC || current->type == OBJECT) continue;
        if(opts->verbose) printf(ANSI_BLUE "exec:" ANSI_GREEN "%s" ANSI_RESET "\n", cmd);
        if(system(cmd))
        {
            errors++;
            printf(ANSI_RED "failed to link target " ANSI_YELLOW "%s" ANSI_RESET "\n", current->ident);
        }
        free(flags);
        free(list);
        free(path);
        free(dir);
        free(cmd);
    }
    if(errors) error("failed to link one or more targets");
}

void install(llist* installtargets)
{
    if(opts->install)
    for(int32 i = 0; i < llist_total(targets, 0); i++)
    {
        target* current = llist_get(targets, i, 0);
        if(!searchstr(installtargets, current->ident) && !searchstr(installtargets, "all")) continue;
        if(!current->install) continue;
        printf(ANSI_MAGENTA "installing " ANSI_YELLOW "%s" ANSI_RESET "\n", current->ident);
        for(int32 j = 0; j < llist_total(current->install, 0); j++)
        {
            if(opts->verbose) printf(ANSI_BLUE "exec:" ANSI_GREEN "%s" ANSI_RESET "\n", (char*)llist_get(current->install, j, 0));
            system((const char*)llist_get(current->install, j, 0));
        }
    }
    if(opts->uninstall)
    for(int32 i = 0; i < llist_total(targets, 0); i++)
    {
        target* current = llist_get(targets, i, 0);
        if(!searchstr(installtargets, current->ident) && !searchstr(installtargets, "all")) continue;
        printf(ANSI_MAGENTA "uninstalling " ANSI_YELLOW "%s" ANSI_RESET "\n", current->ident);
        for(int32 j = 0; j < llist_total(current->uninstall, 0); j++)
        {
            if(opts->verbose) printf(ANSI_BLUE "exec:" ANSI_GREEN "%s" ANSI_RESET "\n", (char*)llist_get(current->uninstall, j, 0));
            system((const char*)llist_get(current->uninstall, j, 0));
        }
    }
}

void handleopts(llist* wanted)
{
    //to the person who looks at the next few lines:
    //here be dragons
    //I am really sorry... don't even try, I am not sure if I can read it myself
    if(opts->printast)
    {
        if(opts->wanted_only)
        {
            for(int32 i = 0; i < tree->children_num; i++)
            {
                if(strcmp(tree->children[i]->tag, "target|>") == 0
                   && (searchstr(wanted, "all") || searchstr(wanted, tree->children[i]->children[1]->contents)))
                    mpc_ast_print(tree->children[i]);
                if(strcmp(tree->children[i]->tag, "system|>") == 0)
                    for(int32 j = 0; j < tree->children_num; j++)
                        if(strcmp(tree->children[i]->children[j]->tag, "target|>") == 0
                           && (searchstr(wanted, "all") || searchstr(wanted, tree->children[i]->children[j]->children[1]->contents)))
                            mpc_ast_print(tree->children[i]->children[j]);
            }
        }
        else mpc_ast_print(tree);
    }
    if(opts->printinfo)
    {
        for(int32 i = 0; i < llist_total(targets, 0); i++)
        {
            target* trg = llist_get(targets, i, 0);
            printf("\ntarget: " ANSI_YELLOW "%s\n" ANSI_RESET, trg->ident);
            switch(trg->type)
            {
                case EXECUTABLE: printf(ANSI_BLUE "type: " ANSI_RESET "executable\n"); break;
                case LIBSHARED:  printf(ANSI_BLUE "type: " ANSI_RESET "shared library\n"); break;
                case LIBSTATIC:  printf(ANSI_BLUE "type: " ANSI_RESET "static library\n"); break;
                case OBJECT:     printf(ANSI_BLUE "type: " ANSI_RESET "object code\n"); break;
            }
            if(trg->output) printf(ANSI_BLUE "output path: " ANSI_RESET "%s\n", trg->output);
            printf(ANSI_BLUE "files:" ANSI_RESET);
            for(int32 y = 0; y < llist_total(trg->files, 0); y++)
            {
                file* current = (file*)llist_get(trg->files, y, 0);
                if(current->depends)
                {
                    printf("%s (%s", current->name, (char*)llist_get(current->depends, 0, 0));
                    for(int x = 1; x < llist_total(current->depends, 0); x++)
                        printf(", %s", (char*)llist_get(current->depends, x, 0));
                    printf("), ");
                }
                else
                    printf("%s, ", current->name);
            }
            printf(ANSI_BLUE "\nflags:" ANSI_RESET);
            for(int32 j = 0; j < llist_total(trg->flags, 0); j++)
                printf("%s,", (char*)llist_get(trg->flags, j, 0));
            printf(ANSI_BLUE "\ndepends:" ANSI_RESET);
            for(int32 j = 0; j < llist_total(trg->depends, 0); j++)
                printf("%s,", (char*)llist_get(trg->depends, j, 0));
            printf(ANSI_BLUE "\nlink:" ANSI_RESET);
            for(int32 j = 0; j < llist_total(trg->link, 0); j++)
                printf("%s,", (char*)llist_get(trg->link, j, 0));
            printf("\n");
        }
    }
}

int8 option(llist** wanted, char** argv, int* argc)
{
    if(!argv[0]) return 1;
    if(argv[0][0] == '-' && strlen(argv[0]) > 1 && argv[0][1] != '-') return 1; //TODO8
    START_OPTION("--ast", opts->printast = 1)
          OPTION("--info", opts->printinfo = 1)
          OPTION("--time", opts->time = 1)
          OPTION("--help", printhelp())
          OPTION("--about", printabout())
          OPTION("--fullrebuild", opts->fullrebuild = 1)
          OPTION("--wanted-only", opts->wanted_only = 1)
          OPTION("--check", opts->only_check = 1)
          OPTION("--verbose", opts->verbose = 1)
          OPTION("clean", cleanup())
          OPTION("install", opts->install = 1)
          OPTION("uninstall", opts->uninstall = 1)
          PARAMETER("--compiler", compiler = (++argv)[0]; (*argc)++)
          PARAMETER("--output", output_path = (++argv)[0]; (*argc)++)
          PARAMETER("--dir", chdir((++argv)[0]); (*argc)++)
          else if(strncmp(argv[0], "--", 2) == 0) printf("unrecognized option: %s\n", argv[0]);
          else
          {
            if(!*wanted) *wanted = llist_new(argv[0]);
            else llist_put(*wanted, argv[0]);
          }
    return 1;
}

int8 checkdepends(llist* files)
{
    int8 flag = 0;
    if(files == NULL) return 0;
    for(int i = 0; i < llist_total(files, 0); i++) if(modified((char*)llist_get(files, i, 0))) flag++;
    return flag;
}

void printhelp()
{
    puts("usage: rusty  [-h]  [--help]  [--ast]  [--info]  [--about] [-viratwn] [-o DIR] "
       "[--output DIR] [-d DIR] [--dir DIR] [-c COMPILER] [--compiler COMPILER] "
       "[-o DIR] [TARGETS] [clean] [install] [uninstall]\n");
    puts("--ast   -a         print the AST of rusty file");
    puts("--info  -i         print basic information about each target");
    puts("--time  -t         measure CPU time");
    puts("--compiler -c name change the compiler used");
    puts("--help  -h         print this help text");
    puts("--about            print the about text");
    puts("--dir   -d         change CWD before looking for rusty file");
    puts("--fullrebuild -r   ignore whether files have been changed or not");
    puts("--wanted-only -w   only print information about wanted targets");
    puts("--check -n         don't compile, just check");
    puts("--verbose -v       print more stuff");
    puts("--output -o        change output path");
    exit(0);
}

void printabout()
{
    puts("Rusty build system, v0.13                                    \n");
    puts("Rusty is a simple build system, which borrows its syntax       ");
    puts("from C2's (github.com/c2lang/c2compiler, c2lang.org) built-in  ");
    puts("build system. Rusty uses Daniel Holden's (orangeduck's) mpc    ");
    puts("for parsing rusty files (file called rusty.txt).             \n");
    puts("In Rusty, building is split into targets. Each target can      ");
    puts("be either executable or a library, both shared or static.      ");
    puts("Syntax reference:                                              ");
    puts("#basic rusty file:                                             ");
    puts("compiler: \"gcc\"; #each rusty file must start with this       ");
    puts("                   #the compiler executable should be          ");
    puts("                   #in the $PATH                               ");
    puts("                   #Rusty currently only supports C,C++ and ASM");
    puts("                   #compilers                                  ");
    puts("target test:       #this is creates a target named test        ");
    puts("  name:\"testapp\";  #set name of the produced binary          ");
    puts("  type:executable; #set what kind of target test is            ");
    puts("  file: \"main.c\";  #add a file                               ");
    puts("  dir: \"testdir\";  #add a directory                        \n");
    puts("The target is then compiled with:                              ");
    puts("$rusty test                                                    ");
    puts("or                                                             ");
    puts("$rusty all                                                   \n");
    puts("Rusty produces two directories: object and output              ");
    puts("Directory object contains object file for each source file.    ");
    puts("Directory output contains the resulting binary of each target  ");
    exit(0);
}

void cleanup()
{
    puts(ANSI_CYAN "cleaning up:" ANSI_BLUE " all files will have to be rebuilt next time" ANSI_RESET);
    deletedir("./.rusty");
    deletedir("./output");
    deletedir("./object");
    puts(ANSI_GREEN "cleanup finished." ANSI_RESET);
    exit(0);
}
