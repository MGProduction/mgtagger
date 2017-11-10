#ifndef _MGTAGGER_PRIVATE_
#define _MGTAGGER_PRIVATE_

// --------------------------------------------------------------------
// "private" headers
// --------------------------------------------------------------------

const char*gettoken(const char*line,char*token,char sep);
void       removeendingcrlf(char*line);
wchar_t    utf8_towchar(const char* s, int* adv);

// --------------------------------------------------------------------

LEMMA     *mgtagger_getunknownlemma(LEX*ulex,const char*fit,int utf8,int rareID);
char       mgtagger_getseparatorchar(const char*line);

// --------------------------------------------------------------------

int        mgtokens_readfromconllu(mgtokens*pieces,FILE*f,char*line,int linesize,int format,int flags,slex*dep);
int        mgtokens_readfromsequence(mgtokens*pieces,const char*line,char separator_char);

// --------------------------------------------------------------------

const char*mgtokens_getlemma(mgtokens*p,int i);
const char*mgtokens_getpos(mgtokens*p,int i);
const char*mgtokens_getbase(mgtokens*p,int i);
const char*mgtokens_getgrammar(mgtokens*p,int i);

// --------------------------------------------------------------------

void       LEX_new(LEX*l);
void       LEX_delete(LEX*l);

int        LEX_read(LEX*lex,FILE*f);

LEMMA     *LEX_find(LEX*l,const char*lemma);

int        LEX_addpos(LEX*l,int i,const char*pos,const char*base,const char*gram,int add);
int        LEX_addex(LEX*l,const char*lemma,const char*pos,const char*base,const char*gram,int add,int flags);
int        LEX_add(LEX*l,const char*lemma,const char*pos,const char*base,const char*gram);

int        LEX_known2unknown(const char*known,char*pattern,int tail,int utf8);
int        LEX_getinflection(const char*fit,const char*base,char*rule,int utf8);

void       LEX_sortbystring(LEX*l);

void       LEX_reduce(LEX*l,int limit);

int        LEX_write(LEX*l,LEX*ulex,const char*name,FILE*f,int way,int utf8);

// --------------------------------------------------------------------

const char*slex_add(slex*s,const char*str);

// --------------------------------------------------------------------

void       NGRAMS_new(NGRAMS*l);
void       NGRAMS_delete(NGRAMS*l);

void       NGRAMS_add(NGRAMS*l,const char*ngram);
void       NGRAMS_addex(NGRAMS*l,const char*ngram,int add,int flags);

NGRAM     *NGRAMS_find(NGRAMS*l,const char*pos);
int        NGRAMS_findID(NGRAMS*l,const char*pos);

// --------------------------------------------------------------------

int        LEMMA_compare(const void*a,const void*b);
int        LEMMACNT_compare(const void*a,const void*b);
int        LEMMALENCNT_compare(const void*a,const void*b);
int        POS_compare(const void*a,const void*b);

// --------------------------------------------------------------------

int        NGRAM_compare(const void*a,const void*b);
int        NGRAM_comparesz(const void*a,const void*b);

// --------------------------------------------------------------------
// ---------------------------------------------------

#endif
