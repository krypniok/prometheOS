#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// HomebrewDB host tool
// File layout: [4:'HBD1'][4:len][len:payload] padded/truncated to 1 MiB by Makefile
// Payload layout matches kernel serializer (32-bit sizes):
// int num_tables; per table: int name_len; name[name_len]; int num_columns;
// int data_types[num_columns]; int num_rows; then rows with columns:
// INTEGER: 4 bytes; STRING: int len + bytes; BLOB: uint32 len + bytes

enum { INTEGER = 0, STRING = 1, BLOB = 2 };

typedef struct { char* name; uint8_t* data; uint32_t size; } FSFile;
typedef struct { FSFile* items; size_t count; } FS;

static int load_file(const char* path, uint8_t** out, size_t* outlen){
    FILE* f = fopen(path, "rb"); if(!f) return 0; fseek(f, 0, SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
    if (n <= 0) { fclose(f); return 0; }
    uint8_t* buf = (uint8_t*)malloc(n); if(!buf){ fclose(f); return 0; }
    if (fread(buf,1,n,f)!=(size_t)n){ free(buf); fclose(f); return 0; }
    fclose(f); *out=buf; *outlen=(size_t)n; return 1;
}

static int save_file(const char* path, const uint8_t* buf, size_t len){
    FILE* f = fopen(path, "wb"); if(!f) return 0; size_t w=fwrite(buf,1,len,f); fclose(f); return w==len; }

static int fs_add_or_update(FS* fs, const char* name, const uint8_t* data, uint32_t size){
    for(size_t i=0;i<fs->count;i++){ if(strcmp(fs->items[i].name,name)==0){ free(fs->items[i].data); fs->items[i].data=(uint8_t*)malloc(size); memcpy(fs->items[i].data,data,size); fs->items[i].size=size; return 1; } }
    fs->items = (FSFile*)realloc(fs->items, (fs->count+1)*sizeof(FSFile));
    fs->items[fs->count].name=strdup(name); fs->items[fs->count].data=(uint8_t*)malloc(size); memcpy(fs->items[fs->count].data,data,size); fs->items[fs->count].size=size; fs->count++; return 1;
}

static int fs_remove(FS* fs, const char* name){
    for(size_t i=0;i<fs->count;i++){ if(strcmp(fs->items[i].name,name)==0){ free(fs->items[i].name); free(fs->items[i].data); memmove(&fs->items[i], &fs->items[i+1], (fs->count-i-1)*sizeof(FSFile)); fs->count--; return 1; } }
    return 0;
}

static void fs_free(FS* fs){ for(size_t i=0;i<fs->count;i++){ free(fs->items[i].name); free(fs->items[i].data);} free(fs->items); fs->items=NULL; fs->count=0; }

static int hbdb_parse(const uint8_t* blob, size_t len, FS* outfs){
    if (len < 8) return 0; uint32_t magic=0, plen=0; memcpy(&magic, blob, 4); memcpy(&plen, blob+4, 4);
    if (magic != 0x48424431 || 8+plen > len) return 0; const uint8_t* p = blob+8; const uint8_t* end = p + plen;
    if (p+4 > end) return 0; int num_tables=0; memcpy(&num_tables,p,4); p+=4;
    for (int t=0;t<num_tables;t++){
        if (p+4 > end) return 0; int name_len=0; memcpy(&name_len,p,4); p+=4; if (p+name_len > end) return 0; char tname[64]; int ncp=name_len<63?name_len:63; memcpy(tname,p,ncp); tname[ncp]='\0'; p+=name_len;
        if (p+4 > end) return 0; int num_cols=0; memcpy(&num_cols,p,4); p+=4; if (num_cols <= 0 || num_cols > 16) return 0;
        int types[16]; if (p+4*num_cols > end) return 0; memcpy(types,p,4*num_cols); p += 4*num_cols;
        if (p+4 > end) return 0; int rows=0; memcpy(&rows,p,4); p+=4;
        int is_fs = (strcmp(tname,"FS")==0 && num_cols==2 && types[0]==STRING && types[1]==BLOB);
        for (int r=0;r<rows;r++){
            // col 0
            int strl=0; if (p+4 > end) return 0; memcpy(&strl,p,4); p+=4; if (p+strl > end) return 0; char* nm = (char*)malloc(strl+1); memcpy(nm,p,strl); nm[strl]='\0'; p+=strl;
            // col 1
            uint32_t bs=0; if (p+4 > end) { free(nm); return 0; } memcpy(&bs,p,4); p+=4; if (p+bs > end) { free(nm); return 0; }
            if (is_fs){ fs_add_or_update(outfs, nm, p, bs); }
            free(nm); p += bs;
        }
    }
    return 1;
}

typedef struct { uint8_t* buf; size_t off; size_t cap; } Buf;
static int buf_init(Buf* b){ b->cap=1024; b->off=0; b->buf=(uint8_t*)malloc(b->cap); return b->buf!=NULL; }
static int buf_ensure(Buf* b, size_t need){ while (b->off + need > b->cap){ size_t nc=b->cap*2; uint8_t* np=(uint8_t*)realloc(b->buf,nc); if(!np) return 0; b->buf=np; b->cap=nc; } return 1; }
static int buf_append(Buf* b, const void* src, size_t n){ if(!buf_ensure(b,n)) return 0; memcpy(b->buf + b->off, src, n); b->off += n; return 1; }

static int hbdb_build(const FS* fs, uint8_t** out, size_t* outlen){
    Buf mo; if(!buf_init(&mo)) return 0;
    int ok=1;
    int tables=1; ok &= buf_append(&mo,&tables,4);
    const char* tname = "FS"; int nl = 2; ok &= buf_append(&mo,&nl,4); ok &= buf_append(&mo,tname,nl);
    int cols=2; ok &= buf_append(&mo,&cols,4); int types[2]={STRING,BLOB}; ok &= buf_append(&mo,types,8);
    int rows=(int)fs->count; ok &= buf_append(&mo,&rows,4);
    for(size_t i=0;i<fs->count && ok;i++){
        int sl = (int)strlen(fs->items[i].name); ok &= buf_append(&mo,&sl,4); ok &= buf_append(&mo,fs->items[i].name,sl);
        uint32_t bs = fs->items[i].size; ok &= buf_append(&mo,&bs,4); ok &= buf_append(&mo,fs->items[i].data,bs);
    }
    if(!ok){ free(mo.buf); return 0; }
    size_t tot = 8 + mo.off; uint8_t* outb=(uint8_t*)malloc(tot); if(!outb){ free(mo.buf); return 0; }
    uint32_t magic=0x48424431; memcpy(outb,&magic,4); uint32_t plen=(uint32_t)mo.off; memcpy(outb+4,&plen,4); memcpy(outb+8,mo.buf,mo.off); free(mo.buf);
    *out=outb; *outlen=tot; return 1;
}

static void usage(void){
    fprintf(stderr,"hbdbtool usage:\n");
    fprintf(stderr,"  hbdbtool ls <dbfile>\n");
    fprintf(stderr,"  hbdbtool cat <dbfile> <name>\n");
    fprintf(stderr,"  hbdbtool put <dbfile> <name> <infile>\n");
    fprintf(stderr,"  hbdbtool get <dbfile> <name> <outfile>\n");
    fprintf(stderr,"  hbdbtool rm  <dbfile> <name>\n");
}

int main(int argc, char** argv){
    if (argc < 3) { usage(); return 1; }
    const char* cmd = argv[1]; const char* dbpath = argv[2];
    uint8_t* file=0; size_t flen=0; FS fs={0};
    if (!load_file(dbpath, &file, &flen)) { // no file: start empty
        // ok for put; for others error
        if (strcmp(cmd,"put")!=0){ fprintf(stderr,"cannot open %s\n", dbpath); return 1; }
    } else {
        if (!hbdb_parse(file, flen, &fs)) { fprintf(stderr,"invalid DB file\n"); free(file); return 1; }
        free(file);
    }

    if (strcmp(cmd,"ls")==0){ for(size_t i=0;i<fs.count;i++) printf("%-24s %u\n", fs.items[i].name, fs.items[i].size); fs_free(&fs); return 0; }
    else if (strcmp(cmd,"cat")==0){ if(argc<4){ usage(); return 1;} const char* name=argv[3]; for(size_t i=0;i<fs.count;i++) if(strcmp(fs.items[i].name,name)==0){ fwrite(fs.items[i].data,1,fs.items[i].size,stdout); fputc('\n',stdout); fs_free(&fs); return 0;} fprintf(stderr,"not found\n"); fs_free(&fs); return 1; }
    else if (strcmp(cmd,"get")==0){ if(argc<5){ usage(); return 1;} const char* name=argv[3]; const char* out=argv[4]; for(size_t i=0;i<fs.count;i++) if(strcmp(fs.items[i].name,name)==0){ if(!save_file(out,fs.items[i].data,fs.items[i].size)){ fprintf(stderr,"write failed\n"); fs_free(&fs); return 1;} fs_free(&fs); return 0;} fprintf(stderr,"not found\n"); fs_free(&fs); return 1; }
    else if (strcmp(cmd,"rm")==0){ if(argc<4){ usage(); return 1;} const char* name=argv[3]; if(!fs_remove(&fs,name)){ fprintf(stderr,"not found\n"); fs_free(&fs); return 1; } }
    else if (strcmp(cmd,"put")==0){ if(argc<5){ usage(); return 1;} const char* name=argv[3]; const char* in=argv[4]; uint8_t* ib=0; size_t il=0; if(!load_file(in,&ib,&il)){ fprintf(stderr,"cannot read %s\n", in); fs_free(&fs); return 1; } fs_add_or_update(&fs,name,ib,(uint32_t)il); free(ib); }
    else { usage(); fs_free(&fs); return 1; }

    uint8_t* outb=0; size_t outl=0; if (!hbdb_build(&fs,&outb,&outl)){ fprintf(stderr,"build failed\n"); fs_free(&fs); return 1; }
    if (!save_file(dbpath,outb,outl)){ fprintf(stderr,"save failed\n"); free(outb); fs_free(&fs); return 1; }
    free(outb); fs_free(&fs); return 0;
}
