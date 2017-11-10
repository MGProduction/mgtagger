#ifndef _MGTAGGER_
#define _MGTAGGER_

// ---------------------------------------------------
// - defines for enabling/disabling features
// ---------------------------------------------------

#define mgtagger_wantbaseforms
#define mgtagger_wantdependencies

// ---------------------------------------------------
// - defines
// ---------------------------------------------------

#define  lemma_size 256
#define  pos_size    10

#define  max_ngrams         24
#define  max_inwards_width   4
#define  max_inwards         2

// ---------------------------------------------------
// - LEX
// ---------------------------------------------------

#define lemma_hifreq 1

typedef struct {
 char**str;
 int   num,grow,size;
}slex;

typedef struct {
 const char*pos;
 const char*base;
 const char*gram;
 int        cnt;
}POS;

typedef struct {
 const char   *lemma; 
 int           cnt;
 unsigned char flags; 
 unsigned char num,size;
 POS          *poss;
}LEMMA;

typedef struct {
 LEMMA*lemmas;
 int   num,size;
 slex  pos,base;
}LEX;

// ---------------------------------------------------
// - NGRAMS
// ---------------------------------------------------

typedef struct {
 char*ngram;
 int  cnt;
}NGRAM;

typedef struct {
 NGRAM*ngrams;
 int   num,size;
 int   cnt;
}NGRAMS;

// ---------------------------------------------------
// - mgtoken 
// ---------------------------------------------------

#define mgtoken_nospaceafter     1
#define mgtoken_lemmaisallocated 2
#define mgtokens_baseisallocated 4

typedef struct {
 // piece information
 char          *fit;
 char          *base;
 const char    *pos;  
 int            textpos;
 unsigned short textlen;
 unsigned char  flags;
 
 LEMMA*        lemma; 
 unsigned char cnt;
#if defined(mgtagger_wantdependencies)
 // dependencies variables 
 int           id;
 const char*   dependencykind;
 int           dependencyid;
#endif 
 // pos tagging variables
 unsigned char inlex; // 0 not in lex 1 in lex 2 in lex but different case
 char          selected,selectedB,selectedF;
 float*        emission;
 float*        fwdprob;
 float*        bkwprob; 
 // pos tagging quality check variable
 char          opos[pos_size]; 
 
}mgtoken;

typedef struct {
 mgtoken*pieces;
 int     num,size;
}mgtokens;

// ---------------------------------------------------

void mgtokens_new(mgtokens*p);
void mgtokens_delete(mgtokens*p);

void mgtokens_reset(mgtokens*p);
void mgtokens_add(mgtokens*p,mgtoken*pp);

// ---------------------------------------------------
// - MGTAGGER 
// ---------------------------------------------------

typedef struct {
 NGRAMS ngrams[max_ngrams];
 NGRAMS inward[2]; 
 LEX    lex,ulex; 
 LEX    inflections;
 LEX    colls; 
 slex   pos;
 int    good,good2;
 int    err,err2,errM;
 int    utf8,rare;
 int   *posusage; 
#if defined(mgtagger_wantdependencies) 
 LEX    deprules;
 slex   dependencies;
#endif  
}mgtagger;

// ---------------------------------------------------

int mgtagger_new(mgtagger*mg,const char*loadname);
int mgtagger_delete(mgtagger*mg);

int mgtagger_postag(mgtagger*mg,mgtokens*pieces,int flags);
int mgtagger_postag_debug(mgtagger*mg,mgtokens*pieces,int flags,FILE*g_log,LEX*g_err);

// ---------------------------------------------------

int mgtagger_genericparser(mgtagger*mg,const char*text,mgtokens*pieces,int flags);

// ---------------------------------------------------

#define  mode_lemmapos_pos      0
#define  mode_lemmapos_conllu   1

// ---------------------------------------------------

int mgtagger_learn(const char*input,const char*mgfile,int format,int skip);

// ---------------------------------------------------

#endif
