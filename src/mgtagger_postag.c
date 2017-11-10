//
//  Copyright 2017 Marco Giorgini All Rights Reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//
// --------------------------------------------------------------------
//
// MG Pos Tagger - a C code generic POS Tagger
//
// Author:  Marco Giorgini, marco.giorgini@gmail.com
// Date:    May 2017
// File:    mgtagger_posttag.c
//          This is the only needed C file, if you just want to use the tagger in your own code, to postag.
//
// This is a very simple generic pos tagger, featuting ngrams with most common word spice, with Viterbi-like code.
// It's able to work with inline pos tagged file (the/DT cat/NN is/VBZ on/IN the/DT table/NN) or with conllu files
// (in which case you can select which feature set to use, and you'll also get base forms in output.
// It generates (and it's able to load) a (text) .mg file - lex + ngrams - so you can easily use different ones.
// It works in utf8 - but you can switch it to codepage (changing this setting in the code)
// To use it you in your code you simply need mgtagger_postag.c + mgtagger_private.h / mgtagger.h
//
// --------------------------------------------------------------------

//  -----------------------------------------------------------------------
//  Not a single line of this source code is anyhow related to the author's
//  daily work - nor this source code reflects in anyway that company 
//  technology, or source libraries
//  -----------------------------------------------------------------------


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "mgtagger.h"

// --------------------------------------------------------------------

#define PROB(a,b) ((float)a)/((float)b)
//#define PROB(a,b) log((double)a)/log((double)b)

// --------------------------------------------------------------------

static int g_guessmore=1;
static int g_uselogs=0;
static int g_usestasforunknown=1;
static int g_useinwards=1;
static int g_utf8asdefault=1;

// --------------------------------------------------------------------

FILE*g_debug;

// --------------------------------------------------------------------
// Helpers functions
// --------------------------------------------------------------------

#define _NXT 0x80
#define _SEQ2 0xc0
#define _SEQ3 0xe0
#define _SEQ4 0xf0
#define _SEQ5 0xf8
#define _SEQ6 0xfc

#define _BOM 0xfeff

int wchar_toutf8 (int ucs2, char * utf8)
{
    if (ucs2 < 0x80) {
        utf8[0] = (char)ucs2;
        utf8[1] = '\0';
        return 1;
    }
    if (ucs2 >= 0x80  && ucs2 < 0x800) {
        utf8[0] = (ucs2 >> 6)   | 0xC0;
        utf8[1] = (ucs2 & 0x3F) | 0x80;
        utf8[2] = '\0';
        return 2;
    }
    if (ucs2 >= 0x800 && ucs2 < 0xFFFF) {
	if (ucs2 >= 0xD800 && ucs2 <= 0xDFFF) {
	    /* Ill-formed. */
	    return -2;//UNICODE_SURROGATE_PAIR;
	}
        utf8[0] = ((ucs2 >> 12)       ) | 0xE0;
        utf8[1] = ((ucs2 >> 6 ) & 0x3F) | 0x80;
        utf8[2] = ((ucs2      ) & 0x3F) | 0x80;
        utf8[3] = '\0';
        return 3;
    }
    if (ucs2 >= 0x10000 && ucs2 < 0x10FFFF) {
	/* http://tidy.sourceforge.net/cgi-bin/lxr/source/src/utf8.c#L380 */
	utf8[0] = 0xF0 | (ucs2 >> 18);
	utf8[1] = 0x80 | ((ucs2 >> 12) & 0x3F);
	utf8[2] = 0x80 | ((ucs2 >> 6) & 0x3F);
	utf8[3] = 0x80 | ((ucs2 & 0x3F));
        utf8[4] = '\0';
        return 4;
    }
    return -1;//UNICODE_BAD_INPUT;
}

wchar_t utf8_towchar(const char* s, int* adv)
{
 const unsigned char* p=(const unsigned char*)s;
	wchar_t u = 0;
	if(adv) *adv = 1;
	if (*p<192)
	{
		u = (int)*p;
	}
	else if (*p<224)
	{
		u = ((int)(*p&(unsigned char)0x1f)<<6)|((int)(p[1]&(unsigned char)0x3f));
		if(adv) *adv = 2;
	}
	else if (*p<240)
	{
		if (p[1]!=0)
		{
			u = ((int)(*p&(unsigned char)0x0f)<<12)|((int)(p[1]&(unsigned char)0x3f)<<6)|((int)(p[2]&(unsigned char)0x3f));
			if(adv) *adv = 3;
		}
	}
	else
	{
		if ((p[1]!=0)&&(p[2]!=0))
		{
			u = ((int)(*p&(unsigned char)0x07)<<18)|((int)(p[1]&(unsigned char)0x3f)<<6)|((int)(p[2]&(unsigned char)0x3f));
			if(adv) *adv = 4;
		}
	}
	return u;
}

size_t utf8_strlen(const char*str)
{
 int l=0;
 while(*str)
  {
   int step;
   utf8_towchar(str,&step);
   str+=step;
   l++;
  }
 return l; 
}

size_t utf8_strlwr(char*str)
{
 int l=0;
 while(*str)
  {
   int step;
   int ch=utf8_towchar(str,&step);
   if(iswupper(ch))
    {
     char sz[8];
     int  len;
     ch=towlower(ch);
     len=wchar_toutf8(ch,sz);
     if(len==step)
      memcpy(str,sz,len);
     else
      {
       memmove(str+len,str+step,strlen(str+step)+1);
       memcpy(str,sz,len);
      } 
     str+=step; 
    }
   else
    str+=step;
   l++;
  }
 return l; 
}

const char*utf8_gettoken(const char*line,char*token,wchar_t sep)
{
 int i=0;
 while(*line)  
  {
   int     step;
   wchar_t code=utf8_towchar(line,&step);
   if(code==sep)
    break;
   else
    { 
     memcpy(token+i,line,step);
     i+=step;
     line+=step;    
    } 
  } 
 token[i]=0;
 while(*line)  
  {
   int     step;
   wchar_t code=utf8_towchar(line,&step);
   if(code!=sep)
    break;
   line+=step;     
  }  
 return line;
}

int isinset(const char*what,const char*in)
{
 int        len=strlen(what);
 const char*p=strstr(in,what);
 while(p)
  {
   if(((p==in)||(p[-1]=='|'))&&((p[len]==0)||(p[len]=='|')))
    return 1;
   else 
    p=strstr(p+1,what);
  }
 return 0; 
}

const char*gettoken(const char*line,char*token,char sep)
{
 while(*line&&(*line!=sep))
  *token++=*line++;
 *token=0;
 while(*line==sep) line++;
 return line;
}

void removeendingcrlf(char*line)
{
 int l=strlen(line);
 while((line[l-1]=='\n')||(line[l-1]=='\r'))
  line[--l]=0;       
}

int ansi_getinflection(const char*fit,const char*base,char*rule)
{
 int same=0;
 while(*fit==*base)
  if(*fit==0)
   break;
  else
   {
    same++;
    fit++;
    base++;
   } 
 if(same<2)
  return 0;   
 if((strlen(fit)>6)||(strlen(base)>6))
  return 0;   
 *rule=0;
 if(*fit)  
  sprintf(rule,"-%d%s",strlen(fit),fit);
 if(*base)
  {
   strcat(rule,"+");
   strcat(rule,base);
  }
 if(*rule)
  return 1;
 else 
  return 0;   
}

int utf8_getinflection(const char*fit,const char*base,char*rule)
{
 const char*lastfit=fit,*lastbase=base;
 int     stepB,stepF,same=0;
 wchar_t chB,chF;
 chB=utf8_towchar(base,&stepB);chF=utf8_towchar(fit,&stepF);
 while(chB==chF)
  if(chB==0)
   break;
  else
   {
    same++;    
    lastfit=fit;fit+=stepF;chF=utf8_towchar(fit,&stepF);
    lastbase=base;base+=stepB;chB=utf8_towchar(base,&stepB);
   } 
 if(same<2)
  return 0;  
 if((utf8_strlen(fit)>6)||(utf8_strlen(base)>6))
  return 0;  
 *rule=0; 
 fit=lastfit;
 base=lastbase;
 if(*fit)  
  sprintf(rule,"-%d%s",strlen(fit),fit);
 if(*base)
  {
   strcat(rule,"+");
   strcat(rule,base);
  }
 if(*rule)
  return 1;
 else 
  return 0;   
}

int applyrule(const char*fit,int fitlen,const char*rule,char*base,int*tail)
{
 int taillen=(rule[1])-'0';
 if(memcmp(fit+fitlen-taillen,rule+2,taillen)==0)
  {
   strcpy(base,fit);
   strcpy(base+fitlen-taillen,rule+3+taillen);
   if(tail)
    *tail=taillen;
   return 1;
  }
 else
  return 0; 
}

int utf8_getbestbaseform(LEX*inflections,const char*fit,const char*pos,char*base)
{
 int  i,j,bestscore=0,fitlen=strlen(fit);
 char bestbase[256];
 *bestbase=0;
 for(i=0;i<inflections->num;i++)
  {
   const char*rule=inflections->lemmas[i].lemma;
   for(j=0;j<inflections->lemmas[i].num;j++)    
    {
     if(strcmp(inflections->lemmas[i].poss[j].pos,pos)==0)
      {
       char base[256];
       int  tail;
       if(applyrule(fit,fitlen,rule,base,&tail)>0)
        {
         if(inflections->lemmas[i].poss[j].cnt+tail*100>bestscore)
          {
           bestscore=inflections->lemmas[i].poss[j].cnt+tail*100;
           strcpy(bestbase,base);
          }
        }
      }
    } 
  }
 if(bestscore>0)
  {
   if(base)
    strcpy(base,bestbase);
   return bestscore;
  }
 else 
  return 0;
}

int ansi_getbestbaseform(LEX*inflections,const char*fit,const char*pos,char*base)
{
 return 0;
}

// --------------------------------------------------------------------
// NGRAMS (actually it's just a basic string dictionary - with count
// --------------------------------------------------------------------

void NGRAMS_new(NGRAMS*l)
{
 l->cnt=0;
 l->num=0;l->size=256;
 l->ngrams=(NGRAM*)calloc(l->size,sizeof(NGRAM));
}

void NGRAMS_addex(NGRAMS*l,const char*ngram,int add,int flags)
{
 int i;
 if(flags&1)
  ;
 else 
  for(i=0;i<l->num;i++)
   if(strcmp(l->ngrams[i].ngram,ngram)==0)
    {
     l->ngrams[i].cnt+=add;
     return;
    }
 if(l->num+1>=l->size)
  {
   l->size+=32;
   l->ngrams=(NGRAM*)realloc(l->ngrams,sizeof(NGRAM)*l->size);
  }
 l->ngrams[l->num].ngram=_strdup(ngram);
 l->ngrams[l->num].cnt=add;
 l->num++; 
}

void NGRAMS_add(NGRAMS*l,const char*ngram)
{
 NGRAMS_addex(l,ngram,1,0);
}

int NGRAM_compare(const void*a,const void*b)
{
 return ((NGRAM*)b)->cnt-((NGRAM*)a)->cnt;
}

int NGRAM_comparesz(const void*a,const void*b)
{
 return strcmp(((NGRAM*)a)->ngram,((NGRAM*)b)->ngram);
}


int NGRAMS_read(FILE*f,NGRAMS*n)
{
 if(f)
  {
   char       line[1024];
   char       value[32];
   int        i,gcnt,cnt;  
   fgets(value,sizeof(value),f);
   gcnt=atoi(value);
   for(i=0;i<gcnt;i++)
    {
     char       ngram[256];
     const char*l;
     fgets(line,sizeof(line),f);
     l=gettoken(line,ngram,'\t');
     l=gettoken(l,value,' ');
     cnt=atoi(value);        
     NGRAMS_addex(n,ngram,cnt,1);
     n->cnt+=cnt;               
    }
   fgets(line,sizeof(line),f);
   qsort(n->ngrams,n->num,sizeof(n->ngrams[0]),NGRAM_comparesz);
   return 1;
  }
 else
  return 0;
}

NGRAM*NGRAMS_find(NGRAMS*l,const char*pos)
{
 NGRAM nn,*pn;
 nn.ngram=(char*)pos;
 pn=(NGRAM*)bsearch(&nn,l->ngrams,l->num,sizeof(l->ngrams[0]),NGRAM_comparesz);
 return pn;
}

int NGRAMS_findID(NGRAMS*l,const char*pos)
{
 NGRAM nn,*pn;
 nn.ngram=(char*)pos;
 pn=(NGRAM*)bsearch(&nn,l->ngrams,l->num,sizeof(l->ngrams[0]),NGRAM_comparesz);
 if(pn)
  return pn-l->ngrams;
 else
  return -1; 
}

void NGRAMS_delete(NGRAMS*l)
{
 int i;
 for(i=0;i<l->num;i++)
  free(l->ngrams[i].ngram);
 free(l->ngrams); 
 memset(l,0,sizeof(NGRAMS));
}

// --------------------------------------------------------------------
// LEXICON (a simple dictionary)
// --------------------------------------------------------------------

void slex_new(slex*s,int size,int grow)
{
 s->num=0;s->grow=grow;
 s->size=size;
 s->str=(char**)calloc(s->size,sizeof(s->str[0]));
}

int slex_delete(slex*s)
{
 int i,size=0;
 for(i=0;i<s->num;i++)
  {
   size+=strlen(s->str[i])+1;
   free(s->str[i]);
  } 
 size+=s->size*sizeof(char*); 
 free(s->str); 
 return size; 
}

const char*slex_add(slex*s,const char*str)
{
 int i;
 for(i=0;i<s->num;i++)
  if(strcmp(s->str[i],str)==0)   
   return s->str[i];    
 if(s->num+1>=s->size)
  {
   s->size+=s->grow;
   s->str=(char**)realloc(s->str,s->size*sizeof(char*));
  }
 s->str[s->num]=_strdup(str);
 return s->str[s->num++];
}

void LEX_new(LEX*l)
{
 l->num=0;l->size=8192;
 l->lemmas=(LEMMA*)calloc(l->size,sizeof(LEMMA));
 slex_new(&l->pos,32,8); 
 slex_new(&l->base,8192,1024); 
}

int LEMMA_compare(const void*a,const void*b)
{
 return strcmp(((LEMMA*)a)->lemma,((LEMMA*)b)->lemma);
}

int LEMMACNT_compare(const void*a,const void*b)
{
 return ((LEMMA*)b)->cnt-((LEMMA*)a)->cnt;
}

int LEMMALENCNT_compare(const void*a,const void*b)
{
 return ((((LEMMA*)b)->cnt)-(utf8_strlen(((LEMMA*)b)->lemma)-3))-((((LEMMA*)a)->cnt)-(utf8_strlen(((LEMMA*)a)->lemma)-3));
}

int POS_compare(const void*a,const void*b)
{
 return ((POS*)b)->cnt-((POS*)a)->cnt;
}

void LEX_sortbystring(LEX*l)
{
 qsort(l->lemmas,l->num,sizeof(l->lemmas[0]),LEMMA_compare);
}

int   LEX_scanutf8(LEX*l,const char*text,char*lemma)
{
 int p,ch=utf8_towchar(text,&p),len=p;
 int i=0,ret=0;
 while(i<l->num)
  {
   int dif=memcmp(l->lemmas[i].lemma,text,len);
   if(dif>0)
    break;
   if(dif<0)
    i++;
   else
    {   
     int nch=utf8_towchar(text+len,&p);  
     if(l->lemmas[i].lemma[len]==0)
      {
       if((iswalpha(ch)&&iswalpha(nch))||(iswdigit(ch)&&iswdigit(nch)))
        ;
       else
        { 
         strcpy(lemma,l->lemmas[i].lemma);
         ret=len;
        } 
      }      
     ch=nch; 
     len+=p; 
    } 
  }
 return ret; 
}

LEMMA*LEX_find(LEX*l,const char*lemma)
{
 LEMMA ll,*pl;
 ll.lemma=(char*)lemma;
 pl=(LEMMA*)bsearch(&ll,l->lemmas,l->num,sizeof(l->lemmas[0]),LEMMA_compare); 
 return pl;
}

int LEX_addpos(LEX*l,int i,const char*pos,const char*base,const char*gram,int add)
{
 LEMMA*w=&l->lemmas[i];
 
 if(pos&&*pos) 
  pos=slex_add(&l->pos,pos);
 
 if(base&&((*base==0)||(strcmp(base,w->lemma)==0)))
   base=NULL;
 else 
  if(base&&*base) 
   base=slex_add(&l->base,base);
 
 w->cnt+=add;
 for(i=0;i<w->num;i++)
  if(strcmp(w->poss[i].pos,pos)==0)
   {
    w->poss[i].cnt+=add;
    break;
   }
 if(i==w->num)
  {
   if(w->num+1>=w->size)
    {
     w->size+=2;
     w->poss=(POS*)realloc(w->poss,sizeof(POS)*w->size);
    }
   w->poss[w->num].cnt=add;
   w->poss[w->num].pos=pos;
   w->poss[w->num].base=base;
   w->poss[w->num].gram=gram;
   w->num++;
  }   
 return i;
}

int LEX_addex(LEX*l,const char*lemma,const char*pos,const char*base,const char*gram,int add,int flags)
{
 if(pos&&*pos) 
  pos=slex_add(&l->pos,pos);
 
 if(base&&((*base==0)||(strcmp(base,lemma)==0)))
   base=NULL;
 else 
  if(base&&*base) 
   base=slex_add(&l->base,base);
 
 if((*lemma==0)||(*pos==0))
  {
   int k;
   k=0;
  }
 if((flags&1)==0)
  {
   int i=l->num;
   while(i--)
    if(strcmp(l->lemmas[i].lemma,lemma)==0)
     {
      LEMMA*w=&l->lemmas[i];
      w->cnt+=add;
      for(i=0;i<w->num;i++)
       if(strcmp(w->poss[i].pos,pos)==0)
        {
         if(base&&w->poss[i].base&&strcmp(w->poss[i].base,base))
          {
           int k;
           k=0;
          }
         w->poss[i].cnt+=add;
         break;
        }
      if(i==w->num)
       {
        if(w->num+1>=w->size)
         {
          w->size+=2;
          w->poss=(POS*)realloc(w->poss,sizeof(POS)*w->size);
         }
        w->poss[w->num].cnt=add;
        w->poss[w->num].pos=pos;
        w->poss[w->num].base=base;
        w->poss[w->num].gram=gram;
        w->num++;
       }   
      return i;
     }
  }   
 if(l->num+1>=l->size)
  {
   l->size+=8192;
   l->lemmas=(LEMMA*)realloc(l->lemmas,sizeof(LEMMA)*l->size);
  }
 l->lemmas[l->num].lemma=_strdup(lemma);
 l->lemmas[l->num].cnt=add;
 l->lemmas[l->num].size=2;
 l->lemmas[l->num].num=0;
 l->lemmas[l->num].flags=0;
 l->lemmas[l->num].poss=(POS*)calloc(l->lemmas[l->num].size,sizeof(POS));
 l->lemmas[l->num].poss[l->lemmas[l->num].num].cnt=add;
 l->lemmas[l->num].poss[l->lemmas[l->num].num].pos=pos;
 l->lemmas[l->num].poss[l->lemmas[l->num].num].base=base;
 l->lemmas[l->num].poss[l->lemmas[l->num].num].gram=gram;
 l->lemmas[l->num].num++;
 l->num++; 
 return l->num-1;
}

int LEX_add(LEX*l,const char*lemma,const char*pos,const char*base,const char*gram)
{
 return LEX_addex(l,lemma,pos,base,gram,1,0);
}

void LEX_reduce(LEX*l,int limit)
{  
 int i;
 qsort(l->lemmas,l->num,sizeof(l->lemmas[0]),LEMMALENCNT_compare);
 if(limit==-2)
  {
   int i=l->num/5,val=l->lemmas[i].cnt;
   while(i&&(l->lemmas[i].cnt==val)) i--;
   l->num=i;
   return;
  }
 else 
 if(limit==-1)
  {
   int sum=0,avg=0;
   for(i=0;i<l->num;i++)
    sum+=l->lemmas[i].cnt;
   avg=sum/l->num;
   limit=avg/10;
   if(limit<1)
    limit=1;
  }  
 for(i=0;i<l->num;i++)
  if(l->lemmas[i].cnt<=limit)
   {
    l->num=i;
    break;
   }
}

int string_compare(const void*a,const void*b)
{
 return strcmp((const char*)a,(const char*)b);
}

int LEX_read(LEX*lex,FILE*f,int lexflags)
{
 if(f)
  {
   char       line[8192];   
   while(!feof(f))
    {
     const char*l;     
     const char*base;
     char       lemma[lemma_size],value[32],posbase[pos_size+lemma_size],pos[pos_size+lemma_size];
     int        gcnt=0,cnt,flags=1;
     int        lemmaid=-1;
     fgets(line,sizeof(line),f);      
     removeendingcrlf(line);
     if(*line==0)
      break;
     else
      {
       l=gettoken(line,lemma,'\t');
       l=gettoken(l,value,' ');
       gcnt=atoi(value);
       while(*l)
        {
         l=gettoken(l,posbase,' ');         
         if((strcmp(posbase,"***")==0)&&(lemmaid!=-1))
          {
           lex->lemmas[lemmaid].flags|=1;          
           break;
          } 
         else 
          { 
           base=gettoken(posbase,pos,'_');       
           l=gettoken(l,value,' ');         
           cnt=atoi(value);           
           if(lexflags&1) cnt=cnt*cnt;
           lemmaid=LEX_addex(lex,lemma,pos,base,NULL,cnt,flags);         
           flags=0;
          } 
        }       
      }  
    }   
   qsort(lex->base.str,lex->base.num,sizeof(char*),string_compare); 
   return 1;
  }
 else
  return 0; 
}

void LEX_delete(LEX*l)
{
 int i,size=0;
 for(i=0;i<l->num;i++)
  {
   size+=sizeof(l->lemmas[i].poss[0])*l->lemmas[i].size;
   free(l->lemmas[i].poss);
   size+=strlen(l->lemmas[i].lemma)+1;
   free((char*)l->lemmas[i].lemma);   
  }
 size+=slex_delete(&l->pos);
 size+=slex_delete(&l->base);
 free(l->lemmas); 
 size+=sizeof(l->lemmas[0])*l->size;
 memset(l,0,sizeof(LEX));
 i=0;
}

int utf8_LEX_known2unknown(const char*known,char*pattern,int tail)
{
 int len=utf8_strlen(known);
 if(len>1+tail)
  {
   int i,ii,dig=0;
   int step;
   for(ii=i=0;(i<len-1-tail)&&(ii<2);i+=step)
    {     
     wchar_t code=utf8_towchar(known+i,&step); 
     if(iswdigit(code))
      {
       if((ii==0)||(pattern[ii-1]!='d'))
        pattern[ii++]='d';
      } 
     else 
     if(iswupper(code))
      {
       if((ii==0)||(pattern[ii-1]!='A'))
        pattern[ii++]='A';     
      }
     else 
     if(iswlower(code)||iswalpha(code))
      {
       if((ii==0)||(pattern[ii-1]!='a'))
        pattern[ii++]='a';     
      }
     else
      if((ii==0)||(pattern[ii-1]!='_'))
       pattern[ii++]='_'; 
    }
     
   pattern[ii++]='*';  
   if((pattern[0]=='d')&&(pattern[1]=='*'))
    {
     int j;
     for(j=1;j<=tail;j+=step)
      {
       wchar_t code=utf8_towchar(known+i,&step); 
       if(iswdigit(code))
        dig++; 
      }  
    }
   if(dig!=tail)
    { 
     int pos=0,tpos=0;
     while(tpos<len-tail)
      {
       wchar_t code=utf8_towchar(known+pos,&step); 
       pos+=step;
       tpos++;
      }
     while(tail)
      {
       wchar_t code=utf8_towchar(known+pos,&step); 
       memcpy(&pattern[ii],&known[pos],step);
       ii+=step;pos+=step;
       tail--;
      } 
    }     
   pattern[ii]=0;
   return 1;
  }
 else
 if(tail==0)
  {
   int i,ii,dig=0,step;
   for(ii=i=0;i<len;i+=step)
    {
     wchar_t code=utf8_towchar(known+i,&step); 
     if(iswdigit(code))
      {
       if((ii==0)||(pattern[ii-1]!='d'))
        pattern[ii++]='d';
      } 
     else 
     if(iswupper(code))
      {
       if((ii==0)||(pattern[ii-1]!='A'))
        pattern[ii++]='A';     
      }
     else 
     if(iswlower(code)||iswalpha(code))
      {
       if((ii==0)||(pattern[ii-1]!='a'))
        pattern[ii++]='a';     
      }
     else
      if((ii==0)||(pattern[ii-1]!='_'))
       pattern[ii++]='_'; 
    }  
   pattern[ii]=0;  
   if(*pattern)
    return 1;
  }
 return 0; 
}

int ansi_LEX_known2unknown(const char*known,char*pattern,int tail)
{
 int len=strlen(known);
 if(len>1+tail)
  {
   int i,ii,dig=0;
   for(ii=i=0;i<len-1-tail;i++)
    if(isdigit((unsigned char)known[i]))
     {
      if((ii==0)||(pattern[ii-1]!='d'))
       pattern[ii++]='d';
     } 
    else 
    if(isupper((unsigned char)known[i]))
     {
      if((ii==0)||(pattern[ii-1]!='A'))
       pattern[ii++]='A';     
     }
    else 
    if(islower((unsigned char)known[i]))
     {
      if((ii==0)||(pattern[ii-1]!='a'))
       pattern[ii++]='a';     
     }
    else
     pattern[ii++]='_'; 
     
   pattern[ii++]='*';  
   if((pattern[0]=='d')&&(pattern[1]=='*'))
    {
     int j;
     for(j=1;j<=tail;j++)
      if(isdigit((unsigned char)known[len-j]))
       dig++; 
    }
   if(dig!=tail)
    { 
     while(tail)
      {
       pattern[ii++]=known[len-tail];
       tail--;
      } 
    } 
    
   pattern[ii]=0;
   return 1;
  }
 else
 if(tail==0)
  {
   int i,ii,dig=0;
   for(ii=i=0;i<len-1-tail;i++)
    if(isdigit((unsigned char)known[i]))
     {
      if((ii==0)||(pattern[ii-1]!='d'))
       pattern[ii++]='d';
     } 
    else 
    if(isupper((unsigned char)known[i]))
     {
      if((ii==0)||(pattern[ii-1]!='A'))
       pattern[ii++]='A';     
     }
    else 
    if(islower((unsigned char)known[i]))
     {
      if((ii==0)||(pattern[ii-1]!='a'))
       pattern[ii++]='a';     
     }
    else
     pattern[ii++]='_'; 
   pattern[ii]=0;  
   if(*pattern)
    return 1;
  }
 return 0; 
}

int LEX_known2unknown(const char*known,char*pattern,int tail,int utf8)
{
 if(utf8)
  return utf8_LEX_known2unknown(known,pattern,tail);
 else 
  return ansi_LEX_known2unknown(known,pattern,tail); 
}

int LEX_getinflection(const char*fit,const char*base,char*rule,int utf8)
{
 if(utf8)
  return utf8_getinflection(fit,base,rule);
 else 
  return ansi_getinflection(fit,base,rule);
}

int LEX_getbestbaseform(LEX*inflections,const char*fit,const char*pos,char*base,int utf8)
{
 if(utf8)
  return utf8_getbestbaseform(inflections,fit,pos,base);
 else 
  return ansi_getbestbaseform(inflections,fit,pos,base);
}

// --------------------------------------------------------------------

// --------------------------------------------------------------------
// POS TAGGING code
// --------------------------------------------------------------------

void mgtokens_new(mgtokens*p)
{
 p->num=0;p->size=256;
 p->pieces=(mgtoken*)calloc(p->size,sizeof(mgtoken));
}

void mgtokens_delete(mgtokens*p)
{
 int i;
 for(i=0;i<p->num;i++)
  {
   free(p->pieces[i].fit);p->pieces[i].fit=NULL;
   if(p->pieces[i].base) 
    if(p->pieces[i].flags&mgtokens_baseisallocated)
     {free(p->pieces[i].base);p->pieces[i].base=NULL;}
  } 
 free(p->pieces); 
}

void mgtokens_reset(mgtokens*p)
{
 int i;
 for(i=0;i<p->num;i++)
  {
   free(p->pieces[i].fit);p->pieces[i].fit=NULL;
   if(p->pieces[i].base) 
    if(p->pieces[i].flags&mgtokens_baseisallocated)
     {free(p->pieces[i].base);p->pieces[i].base=NULL;}
  } 
 p->num=0;
}

void mgtokens_add(mgtokens*p,mgtoken*pp)
{
 if(p->num+1>=p->size)
  {
   p->size+=64;
   p->pieces=(mgtoken*)realloc(p->pieces,p->size*sizeof(mgtoken));
  }
 pp->textlen=strlen(pp->fit); 
 memcpy(&p->pieces[p->num],pp,sizeof(mgtoken));
 p->num++;
}

const char*mgtokens_getpos(mgtokens*p,int i)
{
 if(i<0)
  return "BOF";
 else
 if(i>=p->num)
  return "EOF";
 else
  return p->pieces[i].pos; 
}

const char*mgtokens_getlemma(mgtokens*p,int i)
{
 if(i<0)
  return "BOF";
 else
 if(i>=p->num)
  return "EOF";
 else
  return p->pieces[i].fit;
}

const char*mgtokens_getbase(mgtokens*p,int i)
{
 if(i<0)
  return "BOF";
 else
 if(i>=p->num)
  return "EOF";
 else
  return p->pieces[i].base;
}

const char*mgtokens_getgrammar(mgtokens*p,int i)
{
 if(i<0)
  return "";
 else
 if(i>=p->num)
  return "";
 else
  return "";//p->pieces[i].gram;
}

float mgtagger_gettransition_forward(mgtagger*mg,mgtokens*p,int i,const char*check,int ngrams)
{
 float retvalue=0;
 if((mg->ngrams[ngrams-1].num)&&(i-(ngrams-1)>=-1)&&(i-(ngrams-1)<=p->num))
  {
   char  tpos[pos_size*16+16];
   int   j,l=0;
   NGRAM*n=NULL,*nb;
   *tpos=0;
   if(((ngrams==2)||(ngrams==3)||(ngrams==4))&&(i-(ngrams-1)>=0)&&p->pieces[i-(ngrams-1)].lemma&&(p->pieces[i-(ngrams-1)].lemma->flags&lemma_hifreq)&&g_useinwards)
    {
     for(j=0;j<ngrams-1;j++)
      {
       if(*tpos) strcat(tpos," ");
       if(j==0)
        {
         strcat(tpos,mgtokens_getpos(p,i-(ngrams-j-1)));     
         strcat(tpos,":");
         strcat(tpos,mgtokens_getlemma(p,i-(ngrams-j-1)));     
        }
       else        
        strcat(tpos,mgtokens_getpos(p,i-(ngrams-j-1)));     
      } 
     l=strlen(tpos);  
     if(*tpos) strcat(tpos," ");
     strcat(tpos,check); 
        
     n=NGRAMS_find(&mg->inward[0],tpos);    
     if(n)
      {
       tpos[l]=0;
       nb=NGRAMS_find(&mg->inward[0],tpos); 
       if(nb)            
        retvalue+=(float)n->cnt/(float)nb->cnt;
       else
        retvalue+=(float)n->cnt/(float)mg->inward[0].cnt; 
       return retvalue; 
      }
    }   
   if(1)
    {
     *tpos=0;
     for(j=0;j<ngrams-1;j++)
      {
       if(*tpos) strcat(tpos," ");
       strcat(tpos,mgtokens_getpos(p,i-(ngrams-j-1)));     
      } 
     l=strlen(tpos);  
     if(*tpos) strcat(tpos," ");
     strcat(tpos,check); 
     n=NGRAMS_find(&mg->ngrams[ngrams-1],tpos);                
     if(n)
      {
       tpos[l]=0;
       nb=NGRAMS_find(&mg->ngrams[ngrams-2],tpos); 
       if(nb)    
        retvalue+=(float)n->cnt/(float)nb->cnt;
       else
        retvalue+=(float)n->cnt/(float)mg->ngrams[ngrams-2].cnt; 
      }     
    } 
  }
 return retvalue;
}  

float mgtagger_gettransition_backward(mgtagger*mg,mgtokens*p,int i,const char*check,int ngrams)
{
 float retvalue=0;
 if((mg->ngrams[ngrams-1].num)&&(i>=-1)&&(i+(ngrams-1)<=p->num))
  {
   char  tpos[pos_size*16+16];
   int   j,l=0;
   NGRAM*n=NULL,*nb;
   *tpos=0;
   if(((ngrams==2)||(ngrams==3)||(ngrams==4))&&(i+ngrams-1<p->num)&&p->pieces[i+ngrams-1].lemma&&(p->pieces[i+ngrams-1].lemma->flags&lemma_hifreq)&&g_useinwards)
    {
     strcat(tpos,check);          
     l=strlen(tpos);  
     for(j=1;j<ngrams;j++)
      {
       if(*tpos) strcat(tpos," ");
       if(j==ngrams-1)
        {
         strcat(tpos,mgtokens_getpos(p,i+j));  
         strcat(tpos,":");
         strcat(tpos,mgtokens_getlemma(p,i+j));                  
        }
       else 
        strcat(tpos,mgtokens_getpos(p,i+j));     
      }     
     n=NGRAMS_find(&mg->inward[1],tpos);    
     if(n)
      {
       nb=NGRAMS_find(&mg->inward[1],tpos+l+1); 
       if(nb)            
        retvalue+=(float)n->cnt/(float)nb->cnt;
       else
        retvalue+=(float)n->cnt/(float)mg->inward[1].cnt; 
       return retvalue; 
      }
    }   
   if(1)
    { 
     strcpy(tpos,check);
     l=strlen(tpos);  
     for(j=1;j<ngrams;j++)
      {
       if(*tpos) strcat(tpos," ");
       strcat(tpos,mgtokens_getpos(p,i+j));     
      }     
     n=NGRAMS_find(&mg->ngrams[ngrams-1],tpos);                
     if(n)
      {
       nb=NGRAMS_find(&mg->ngrams[ngrams-2],tpos+l+1); 
       if(nb)            
        retvalue+=(float)n->cnt/(float)nb->cnt;
       else
        retvalue+=(float)n->cnt/(float)mg->ngrams[ngrams-2].cnt; 
      }     
    } 
  }
 return retvalue;
}  

LEMMA*mgtagger_getunknownlemma(LEX*ulex,const char*fit,int utf8,int rareID)
{
 LEMMA*plemma2=NULL;
 char  ulemmaC[256];
 int   got=0,tail=5;
 while((plemma2==NULL)&&(tail>=0))
  {
   if((plemma2==NULL)&&LEX_known2unknown(fit,ulemmaC,tail,utf8))
    plemma2=LEX_find(ulex,ulemmaC);                               
   tail--; 
  }  
 if(plemma2==NULL)
  {
   if(rareID!=-1)
    plemma2=&ulex->lemmas[rareID];
   else
    ; 
  }  
 else
  got++;                    
 return plemma2; 
}

LEMMA*mgtagger_getlemma(mgtagger*mg,const char*fit,int force,unsigned char*inlex)
{
 LEMMA*tlemma=LEX_find(&mg->lex,fit);         
 int   bU;
 if(mg->utf8)
  bU=iswupper(utf8_towchar(fit,NULL));
 else
  bU=isupper((unsigned char)*fit);
 if(tlemma)
  {
   if(inlex) 
    if((mg->lex.num<8192)||(tlemma->cnt>1))
     *inlex=1;
    else  
     *inlex=8;        
   return tlemma;
  }
 else
  if(bU)
   {
    char lfit[256];
    strcpy(lfit,fit);
    if(mg->utf8)
     utf8_strlwr(lfit);
    else 
     _strlwr(lfit);
    tlemma=LEX_find(&mg->lex,lfit);         
    if(tlemma)
     {
      if(inlex) *inlex=16;
      return tlemma;
     }     
   }
 if(inlex) *inlex=0;
 if(force)
  {
   tlemma=mgtagger_getunknownlemma(&mg->ulex,fit,mg->utf8,mg->rare);   
   if(tlemma)
    return tlemma;
  }
 return NULL; 
}

int mgtagger_new(mgtagger*mg,const char*loadname)
{
 int ret=0,i;
 
 memset(mg,0,sizeof(mgtagger));
 
 LEX_new(&mg->lex);   
 LEX_new(&mg->ulex); 
 
 slex_new(&mg->pos,32,8);
 slex_new(&mg->dependencies,32,8);
 
 for(i=0;i<max_ngrams;i++)
  NGRAMS_new(&mg->ngrams[i]);
 for(i=0;i<max_inwards;i++) 
  NGRAMS_new(&mg->inward[i]);
 
 mg->utf8=g_utf8asdefault;
 mg->rare=-1;
    
 if(loadname&&*loadname)
  {
   FILE*f=fopen(loadname,"rb");
   if(f)
    {
     while(!feof(f))
      {
       char line[8192];
       fgets(line,sizeof(line),f);  
       removeendingcrlf(line);
       if((*line==0)||(*line=='#'))
        continue;
       else
       if(*line=='$')
        {
         char       key[256];
         const char*value=gettoken(line+1,key,'=');
         // handle vars here
        }
       else
       if(memcmp(line,"[LEX:main",8)==0)
        ret+=LEX_read(&mg->lex,f,1);
       else
       if(memcmp(line,"[LEX:unk",8)==0)
        ret+=LEX_read(&mg->ulex,f,0); 
       else
       if(memcmp(line,"[LEX:col",8)==0)
        {
         LEX_new(&mg->colls); 
         ret+=LEX_read(&mg->colls,f,0);
        } 
       else       
       if(memcmp(line,"[LEX:dep",8)==0)
        {
         LEX_new(&mg->deprules); 
         ret+=LEX_read(&mg->deprules,f,0);
        } 
       else       
       if(memcmp(line,"[INWARDS:",9)==0)
        ret+=NGRAMS_read(f,&mg->inward[atoi(line+9)-1]);
       else
       if(memcmp(line,"[NGRAMS:",8)==0)
        ret+=NGRAMS_read(f,&mg->ngrams[atoi(line+8)-1]);
       else
       if(memcmp(line,"[NET:",5)==0)
        {
        }
       else
        break;
      }  
     fclose(f);
     
     if(1)
      {
       char extralex[256];
       strcpy(extralex,loadname);strcat(extralex,".lex");
       f=fopen(extralex,"rb");
       if(f)
        {
         while(!feof(f))
          {
           char line[8192];
           fgets(line,sizeof(line),f);  
           removeendingcrlf(line);
           if((*line==0)||(*line=='#'))
            continue;
           else
           if(*line=='$')
            {
             char       key[256];
             const char*value=gettoken(line+1,key,'=');
             // handle vars here
            }
           else
           if(memcmp(line,"[LEX:main",8)==0)
            ret+=LEX_read(&mg->lex,f,1);
           else
            break;
          }
         fclose(f); 
        }   
       LEX_sortbystring(&mg->lex);
      }  
        
     if(mg->lex.num)
      {
       int i,j,infl=0;
       mg->posusage=(int*)calloc(mg->ngrams[0].num,sizeof(int));
       for(i=0;i<mg->lex.num;i++)      
        {
         LEMMA*lm=&mg->lex.lemmas[i];
         for(j=0;j<lm->num;j++)
          {
           int posID=NGRAMS_findID(&mg->ngrams[0],lm->poss[j].pos);
           if(posID!=-1)
            mg->posusage[posID]++;
           if(mg->lex.base.num) 
            if(lm->poss[j].base&&*lm->poss[j].base)
             {
              char rule[256];
              if(LEX_getinflection(lm->lemma,lm->poss[j].base,rule,mg->utf8))
               {
                if((infl++)==0)
                 LEX_new(&mg->inflections); 
                LEX_add(&mg->inflections,rule,lm->poss[j].pos,NULL,NULL);
               } 
             } 
            else 
             {
              char rule[256];
              if(LEX_getinflection(lm->lemma,lm->lemma,rule,mg->utf8))
               {
                if((infl++)==0)
                 LEX_new(&mg->inflections); 
                LEX_add(&mg->inflections,rule,lm->poss[j].pos,NULL,NULL);
               } 
             }
          }
        }  
       if(infl&&mg->inflections.num)
        {
         LEX_reduce(&mg->inflections,-1); 
         #if defined(DEBUG_DUMP)
         if(1)
          {
           FILE*f=fopen("inflections.txt","wb+");
           LEX_write(&mg->inflections,NULL,"inflections",f,1,mg->utf8);
           fclose(f);
          }
         #endif 
        } 
      }
     if(mg->ulex.num)
      {
       int i,topcnt;
       for(topcnt=i=0;i<mg->ulex.num;i++)
        if(mg->ulex.lemmas[i].cnt>topcnt)
         {
          mg->rare=i;
          topcnt=mg->ulex.lemmas[i].cnt;
         }       
      }
    } 
  }
 else
  {
   LEX_new(&mg->colls); 
   LEX_new(&mg->inflections);   
   ret=3;  
  } 
 
 mg->good=mg->good2=mg->err=mg->err2=mg->errM=0;  
 
 return (ret>=3);
}

int mgtagger_delete(mgtagger*mg)
{
 int i;
 
 slex_delete(&mg->dependencies);
 slex_delete(&mg->pos);
 
 LEX_delete(&mg->inflections); 
 LEX_delete(&mg->colls); 
 LEX_delete(&mg->ulex); 
 LEX_delete(&mg->lex); 

 for(i=0;i<max_ngrams;i++)
  NGRAMS_delete(&mg->ngrams[i]);

 for(i=0;i<max_inwards;i++)
  NGRAMS_delete(&mg->inward[i]);
  
 free(mg->posusage); 
 mg->posusage=NULL;
  
 return 1;
}

float mgtagger_getbestpos_backward(mgtagger*mg,mgtokens*pieces,int i,int deep,char*selected)
{
 int     odd=0;
 mgtoken*piece=&pieces->pieces[i];
 LEMMA  *plemma=piece->lemma;   
 float   bestprob=0;
 
 if(selected) *selected=-1; 
 
 if(plemma)
  {
   int  t;
   float sum=0;
   for(t=0;t<piece->cnt;t++)
    {
     NGRAM      *next=NULL;
     float       emission,transition=0,prob=0,nextprob=0;
     const char *check=plemma->poss[t].pos;
     int         tcnt=plemma->poss[t].cnt,transitioncount=0;
     
     piece->pos=check;
     
     emission=piece->emission[t];  
            
     transition=mgtagger_gettransition_backward(mg,pieces,i,check,2); 
     if(transition==0)
      {
       NGRAM*uni=NGRAMS_find(&mg->ngrams[0],check);
       transition=(float)0.25f/(float)uni->cnt;
      }
     else       
      {
       int j;
       for(j=3;j<=max_ngrams;j++)
        {
         float value=mgtagger_gettransition_backward(mg,pieces,i,check,j); 
         if(value)
          transition+=value;
         else
          break; 
        }
       transitioncount=(j-2); 
      } 
                                                
     if(nextprob) transition+=nextprob;  
           
     if(transitioncount>1)
      transition/=transitioncount;        
     
     prob=(emission*transition);                     
     
     if((deep<2)&&(i>0))
      {
       char  oselected;
       float nprob;
       nprob=mgtagger_getbestpos_backward(mg,pieces,i-1,deep+1,&oselected);
       prob*=nprob;
      }     
     
     if(prob>bestprob)  
      {
       if(selected) *selected=t;
       bestprob=prob;
      }      
                       
     piece->bkwprob[t]=prob;
     
    }           
   // normalize bkwprob
   for(t=0;t<piece->cnt;t++) 
    sum+=piece->bkwprob[t];
   
   for(t=0;t<piece->cnt;t++) 
    piece->bkwprob[t]=piece->bkwprob[t]*100.0f/sum;    
     
   if(selected)
    piece->pos=piece->lemma->poss[*selected].pos;      
  } 
 else
  odd++; 

 return bestprob; 
}

float mgtagger_getbestpos_forward(mgtagger*mg,mgtokens*pieces,int i,int deep,char*selected)
{
 int     odd=0;
 mgtoken*piece=&pieces->pieces[i];
 LEMMA  *plemma=piece->lemma;   
 float   bestprob=-1000;
 
 if(selected) *selected=-1; 
 
 if(plemma)
  {
   int   t;
   float sum=0;
   for(t=0;t<piece->cnt;t++)
    {
     NGRAM*next=NULL;
     float      emission,transition=0,prob=0,nextprob=0;
     const char*check=plemma->poss[t].pos;
     int        tcnt=plemma->poss[t].cnt,transitioncount=0;
     
     piece->pos=check;
     
     emission=piece->emission[t];  
          
     transition=mgtagger_gettransition_forward(mg,pieces,i,check,2); 
     
     if(0&&g_debug)
      {
       char out[256];
       sprintf(out,"%s %s %.4f %.4f\r\n",piece->fit,piece->pos,emission,transition);                     
       fprintf(g_debug,out);                     
      } 
     
     if(transition==0)
      {
       NGRAM*uni=NGRAMS_find(&mg->ngrams[0],check);
       transition=(float)0.25f/(float)uni->cnt;
      }
     else       
      {
       int j;
       for(j=3;j<=max_ngrams;j++)
        {
         float value=mgtagger_gettransition_forward(mg,pieces,i,check,j); 
         if(value)
          transition+=value;
         else
          break; 
        }
       transitioncount=(j-2); 
      } 
                                                
     if(nextprob) transition+=nextprob;  
          
     if(transitioncount>1)
      transition/=transitioncount;        
     
     if(g_uselogs)
      prob=logf(emission*transition*1000.0f);        
     else
      prob=(emission*transition);        
     
     if((deep<2)&&(i+1<pieces->num))
      {
       char  oselected;
       float nprob;       
       nprob=mgtagger_getbestpos_forward(mg,pieces,i+1,deep+1,&oselected);
       if(g_uselogs)
        prob=(prob+nprob)/2;
       else
        prob*=nprob;
      }                      
     
     if(prob>bestprob)  
      {
       if(selected) *selected=t;
       bestprob=prob;
      }              
     
     if(0&&g_debug)
      {
       char out[256];
       sprintf(out,"%s %s %.4f %.4f\r\n",piece->fit,piece->pos,prob,transition);                     
       fprintf(g_debug,out);                     
      } 
                       
     piece->fwdprob[t]=prob;     
    }           
   // normalize bkwprob
   for(t=0;t<piece->cnt;t++) 
    sum+=piece->fwdprob[t];
   for(t=0;t<piece->cnt;t++) 
    piece->fwdprob[t]=piece->fwdprob[t]*100.0f/sum;    
   
   if(selected)
    piece->pos=piece->lemma->poss[*selected].pos;         
  } 
 else
  odd++; 

 return bestprob; 
}

int mgtagger_getunknownlemmasprobfromcontext(mgtagger*mg,mgtokens*pieces,int i,int*unkcnt,int flags)
{ 
 int cnt=0,fnd=0; 
 if((i>=0)&&(i<pieces->num))
  {
   const char*fit=pieces->pieces[i].fit;
   int        a,ac,x,b,bc,skip=0,top=0;
   for(x=0;x<mg->ngrams[0].num;x++)
    if(mg->posusage[x]>top)
     top=mg->posusage[x];
        
   if(flags&1)
    {
     LEMMA*plemma2=NULL;
     char  ulemmaC[256];
     int   got=0,tail=5,emit=0;
     while((tail>=0)&&(emit<3))
      {
       if(LEX_known2unknown(fit,ulemmaC,tail,mg->utf8))
        {
         plemma2=LEX_find(&mg->ulex,ulemmaC);      
         if(plemma2)
          {
           int i;
           for(i=0;i<plemma2->num;i++)
            {
             for(x=0;x<mg->ngrams[0].num;x++)
              if(strcmp(plemma2->poss[i].pos,mg->ngrams[0].ngrams[x].ngram)==0)
               {
                unkcnt[x]+=plemma2->poss[i].cnt;
                break;
               } 
            }
           emit++; 
          }                         
        } 
       tail--; 
      }  
     if((emit==0)&&(mg->rare!=-1))
      {
       plemma2=&mg->ulex.lemmas[mg->rare];
       if(plemma2)
        {
         int i;
         for(i=0;i<plemma2->num;i++)
          {
           for(x=0;x<mg->ngrams[0].num;x++)
            if(strcmp(plemma2->poss[i].pos,mg->ngrams[0].ngrams[x].ngram)==0)
             {
              unkcnt[x]+=plemma2->poss[i].cnt;
              break;
             }
          }
         emit++; 
        }                         
      }  
     else
      got++;   
     
     if(1)
      {
       int i,topv=0; 
       for(i=0;i<mg->ngrams[0].num;i++)
        if(unkcnt[i]>topv)
         topv=unkcnt[i];
       if(topv>0)  
        for(i=0;i<mg->ngrams[0].num;i++)
         if(unkcnt[i])
          unkcnt[i]=(int)(((float)unkcnt[i]/(float)topv)*100.0f);
      }  
    }  
     
   if(i>0)
    ac=pieces->pieces[i-1].cnt;  
   else
    ac=1; 
   if(i+1<pieces->num) 
    bc=pieces->pieces[i+1].cnt;
   else
    bc=1; 
   for(a=0;a<ac;a++)
    for(b=0;b<bc;b++)
     {
      char tpos[256];
      fnd++;
      for(x=0;x<mg->ngrams[0].num;x++)
       if(mg->posusage[x]>top/2)
        {                
         NGRAM*n;
         if(i-1<0)
          strcpy(tpos,"BOF");
         else
          strcpy(tpos,pieces->pieces[i-1].lemma->poss[a].pos); 
         strcat(tpos," ");      
         strcat(tpos,mg->ngrams[0].ngrams[x].ngram);
         strcat(tpos," ");
         if(i+1<pieces->num) 
          strcat(tpos,pieces->pieces[i+1].lemma->poss[b].pos);
         else
          strcat(tpos,"EOF"); 
         n=NGRAMS_find(&mg->ngrams[2],tpos);
         if(n!=NULL)
          {
           if(n->cnt>25)
            unkcnt[x]+=25;          
           else
            unkcnt[x]+=n->cnt;          
          }  
        }  
       else
        skip++; 
     }  
  }
  
 if(flags&1)
  {
   int i,topv=0; 
   for(i=0;i<mg->ngrams[0].num;i++)
    if(unkcnt[i]>topv)
     topv=unkcnt[i];
   if(topv>0)  
    for(i=0;i<mg->ngrams[0].num;i++)
     if(unkcnt[i])
      unkcnt[i]=(int)(((float)unkcnt[i]/(float)topv)*100.0f);
  }   

 for(i=0;i<mg->ngrams[0].num;i++)
  {
   if(fnd)
    unkcnt[i]/=fnd;
   if(unkcnt[i])
    cnt++;
  }  
 return cnt;   
}

int mgtagger_postag_initialize(mgtagger*mg,mgtokens*pieces)
{
 int i,hasdebuginfo=0,odd=0;
 int*unkcnt=(int*)calloc(sizeof(int),mg->ngrams[0].num);
 // known words
 for(i=0;i<pieces->num;i++)
  {
   mgtoken*piece=&pieces->pieces[i];
   LEMMA  *plemma=piece->lemma=mgtagger_getlemma(mg,piece->fit,1,&piece->inlex);   
   if(piece->lemma&&(piece->inlex==1))
    {
     piece->pos="";     
     piece->cnt=plemma->num;
    }
   // check if there's a pretagged info in piece structure    
   if(*piece->opos) hasdebuginfo++;        
  }  
 // unknown words 
 for(i=0;i<pieces->num;i++)
  {
   mgtoken*piece=&pieces->pieces[i];
   LEMMA  *plemma=piece->lemma;
   if((piece->inlex!=1))
    {
     int   t;                           
     piece->pos="";     
     memset(unkcnt,0,sizeof(int)*mg->ngrams[0].num);
     if(g_usestasforunknown&&mgtagger_getunknownlemmasprobfromcontext(mg,pieces,i,unkcnt,g_guessmore))
      {
       LEMMA*n;
       int   tt,gtcnt=0,maxunkcnt=0;
       // if there's a reference word (different case), use also it stats
       if(plemma)
        for(t=0;t<plemma->num;t++)
         {
          int tcnt=plemma->poss[t].cnt;
          if(((plemma->num>8)&&(tcnt==1))||(t>32))
           break;   
          for(tt=0;tt<mg->ngrams[0].num;tt++)
           if(strcmp(plemma->poss[t].pos,mg->ngrams[0].ngrams[tt].ngram)==0)
            {
             unkcnt[tt]+=100+tcnt;
             break;
            }
         }
       // create a new LEMMA structure  
       n=(LEMMA*)calloc(1,sizeof(LEMMA)); 
       n->lemma=piece->fit;       
       n->num=0;
       // calculate the max value for its POSs
       for(tt=0;tt<mg->ngrams[0].num;tt++)
        if(unkcnt[tt])
         { 
          if(unkcnt[tt]>maxunkcnt)
           maxunkcnt=unkcnt[tt];
         }
       // choose only the best ones
       for(tt=0;tt<mg->ngrams[0].num;tt++)
        if(unkcnt[tt]>maxunkcnt/10)
         {          
          n->num++;
          gtcnt+=unkcnt[tt];
         }
       if(n->num==0)
        {
         int err;
         err=1;
        }            
       n->cnt=gtcnt;  
       n->size=n->num;   
       n->poss=(POS*)calloc(n->size,sizeof(POS)); 
       for(n->num=tt=0;tt<mg->ngrams[0].num;tt++)
        if(unkcnt[tt]>maxunkcnt/10)
         {
          n->poss[n->num].pos=mg->ngrams[0].ngrams[tt].ngram;
          n->poss[n->num].cnt=unkcnt[tt];
          n->num++;
         }      
       qsort(n->poss,n->num,sizeof(n->poss[0]),POS_compare);            
       plemma=piece->lemma=n;
       piece->cnt=n->num;
       piece->flags|=mgtoken_lemmaisallocated;
      }
     else
      {
       plemma=piece->lemma=mgtagger_getlemma(mg,piece->fit,1,&piece->inlex);   
       piece->lemma=plemma;
       for(t=0;t<plemma->num;t++)
        {
         int tcnt=plemma->poss[t].cnt;
         if(((plemma->num>8)&&(tcnt==1))||(t>32))
          break;         
        }
       piece->cnt=t;   
      }           
    }          
  }   
 // allocate buffers and calculate emission
 for(i=0;i<pieces->num;i++)
  {
   mgtoken*piece=&pieces->pieces[i];
   int     t;
   piece->emission=(float*)calloc(piece->cnt,sizeof(float));      
   for(t=0;t<piece->cnt;t++)
    {
     NGRAM*uni;
     LEMMA*plemma=piece->lemma;
     int   tcnt=plemma->poss[t].cnt;
     uni=NGRAMS_find(&mg->ngrams[0],plemma->poss[t].pos);
     if(0)
      {
       piece->emission[t]=PROB(tcnt,piece->lemma->cnt);  
       if(uni) 
        piece->emission[t]*=PROB(uni->cnt,mg->ngrams[0].cnt);
      }         
     else
      {
       piece->emission[t]=PROB(tcnt,uni->cnt);  
       if(0&&uni) 
        piece->emission[t]*=PROB(uni->cnt,mg->ngrams[0].cnt);
       else 
        odd++; 
      }  
    }   
   
   piece->bkwprob=(float*)calloc(piece->cnt,sizeof(float)); 
   piece->fwdprob=(float*)calloc(piece->cnt,sizeof(float));       
  }  
 free(unkcnt);
 return hasdebuginfo; 
}

void mgtagger_postag_setdependencies(mgtagger*mg,mgtokens*pieces)
{
}

void mgtagger_postag_setbaseforms(mgtagger*mg,mgtokens*pieces)
{
 int i;
 for(i=0;i<pieces->num;i++)
  {
   mgtoken*piece=&pieces->pieces[i];    
   if(piece->selected!=-1)
    {
     piece->pos=piece->lemma->poss[piece->selected].pos;
     piece->base=(char*)piece->lemma->poss[piece->selected].base;
     if((piece->base==NULL)||(*piece->base==0))
      if((piece->inlex==1)||(piece->inlex==2))
       piece->base=piece->fit;
      else 
       {
        char base[256];
        if(LEX_getbestbaseform(&mg->inflections,piece->fit,piece->pos,base,mg->utf8)>0)
         if(strcmp(piece->fit,base)==0)
          piece->base=piece->fit;
         else
          { 
           piece->base=_strdup(base);
           piece->flags|=mgtokens_baseisallocated;
          }
       }  
    }
  }
}

void mgtagger_postag_calcstatsanddumperrors(mgtagger*mg,mgtokens*pieces,int flags,FILE*g_log,LEX*g_err)
{
 int i,err=0;
 for(i=0;i<pieces->num;i++)
  {
   mgtoken*piece=&pieces->pieces[i];    
   if(*piece->opos)
    {
     const char*sel="*";
     int        same;
     if(piece->selected!=-1)
      sel=piece->lemma->poss[piece->selected].pos;
     same=strcmp(sel,piece->opos);
     if((same!=0)&&(flags&256)) 
      {
       char*p1=strstr(sel,".");
       char*p2=strstr(piece->opos,".");
       if(p1&&p2&&((p1-sel)==(p2-piece->opos)))
        same=memcmp(sel,piece->opos,(p1-sel));
      }
     if(same==0)
      {
       if(piece->inlex)
        mg->good++;
       else
        mg->good2++; 
      }  
     else
      {
       if(g_err)
        {
         char out[256];
         sprintf(out,"%s-%s",sel,piece->opos);
         LEX_add(g_err,piece->fit,out,NULL,NULL);      
        } 
       if(g_log)
        {             
         int j,miss=0;    
         
         for(j=0;j<piece->cnt;j++)
          if(strcmp(piece->lemma->poss[j].pos,piece->opos)==0)
           break;
         if(j==piece->cnt)
          {mg->errM++;miss++;}
         
         if(err==0)
          {                      
           fprintf(g_log,"CLAUSE: ");        
           for(j=0;j<pieces->num;j++)
            fprintf(g_log,"%s ",pieces->pieces[j].fit);               
           fprintf(g_log,"\r\n");                
          }
         if(piece->inlex)
          fprintf(g_log,"%s POS:%s SEL:%s\r\n",piece->fit,piece->opos,sel);             
         else
          fprintf(g_log,"%s (unknown) POS:%s SEL:%s\r\n",piece->fit,piece->opos,sel);                        
         for(j=0;j<piece->cnt;j++)
          fprintf(g_log,"%s %.2f%%/%.2f%% ",piece->lemma->poss[j].pos,piece->fwdprob[j],piece->bkwprob[j]); 
         if(miss) 
          fprintf(g_log," [MISS]"); 
         if(piece->selectedB==piece->selectedF)
          fprintf(g_log," [SAME]"); 
         fprintf(g_log,"\r\n"); 
         err++;
        } 
       if(piece->inlex)
        mg->err++; 
       else
        mg->err2++; 
      }   
    }   
  }  
 if(g_log&&err) 
  fprintf(g_log,"\r\n");                        
}   

void mgtagger_postag_reset(mgtagger*mg,mgtokens*pieces)
{
 int i;
 for(i=0;i<pieces->num;i++)
  {
   mgtoken*piece=&pieces->pieces[i];
   if(piece->flags&mgtoken_lemmaisallocated)
    {
     free(piece->lemma->poss);
     free(piece->lemma);
    } 
   free(piece->emission);
   free(piece->fwdprob);
   free(piece->bkwprob);
  } 
}  

int notsingle,single;

int mgtagger_reduce_ambiguity(mgtagger*mg,mgtokens*pieces,int i)
{
 return 1;
}

int mgtagger_postag_debug(mgtagger*mg,mgtokens*pieces,int flags,FILE*g_log,LEX*g_err)
{
 int   ret=0,i,got=0,hasdebuginfo=0,mode=1|2|4;
 float gprob=1;
 
 // search for fit and calculate emission
 // allocate temp buffers
 hasdebuginfo=mgtagger_postag_initialize(mg,pieces);
 
  
 if(mode&4)
  { 
   // check for single pos or riche elements
   for(i=0;i<pieces->num;i++)
    {
     mgtoken*piece=&pieces->pieces[i];
     if(piece->cnt==1)
      single+=mgtagger_reduce_ambiguity(mg,pieces,i);
     else 
      notsingle++; 
    } 
  }
 
 if(mode&1)
  {
   // calculate forward probabilities 
   for(i=0;i<pieces->num;i++)
    {
     mgtoken*piece=&pieces->pieces[i];
     float   value=mgtagger_getbestpos_forward(mg,pieces,i,0,&piece->selectedF);         
     piece->selected=piece->selectedF;     
    }
   i=0; 
  }
 if(mode&2)
  {
   // calculate backward probabilities 
   for(i=pieces->num-1;i>=0;i--)
    {
     mgtoken*piece=&pieces->pieces[i];
     mgtagger_getbestpos_backward(mg,pieces,i,0,&piece->selectedB);    
     piece->selected=piece->selectedB;
    } 
  }  
 if((mode&(1|2))==(1|2))
  {
   int same=0;
   // find best combined probabilities 
   for(i=0;i<pieces->num;i++)
    {
     mgtoken*piece=&pieces->pieces[i];
     if(piece->selectedB==piece->selectedF)
      {
       piece->selected=piece->selectedF;
       same++;
      } 
     else
      { 
       int     t;
       float   bestprob=0;
       piece->selected=-1;
       for(t=0;t<piece->cnt;t++)
        {
         //float prob=logf(piece->fwdprob[t])+logf(piece->bkwprob[t]);
         float prob=piece->fwdprob[t]*piece->bkwprob[t];
         if(prob>bestprob)
          {
           bestprob=prob;
           piece->selected=t;
          }
        } 
      }  
     if(piece->selected==-1)
      {
       int k;
       k=0;
      } 
    }
  }  
  
 if(mg->inflections.num)
  mgtagger_postag_setbaseforms(mg,pieces); 
  
 if(mg->deprules.num)
  mgtagger_postag_setdependencies(mg,pieces);  
  
 // if we got pos tagging in input pieces, then calculate quality stats
 // and dump debug info if requested 
 if(hasdebuginfo)
  mgtagger_postag_calcstatsanddumperrors(mg,pieces,flags,g_log,g_err);
  
 // free temp buffers 
 mgtagger_postag_reset(mg,pieces);
 
 return ret;
}

int mgtagger_postag(mgtagger*mg,mgtokens*pieces,int flags)
{
 return mgtagger_postag_debug(mg,pieces,flags,NULL,NULL);
}

int mgtagger_genericparser(mgtagger*mg,const char*text,mgtokens*pieces,int flags)
{
 const char*otext=text;
 mgtokens_reset(pieces);
 while(*text)
  {
   if(mg->utf8)
    {     
     int   step,ch=utf8_towchar(text,&step);     
     if(iswspace(ch))
      text+=step;
     else
      {
       mgtoken p;
       int     len=0;
       char    lemma[256];
       memset(&p,0,sizeof(p));
       p.textpos=text-otext;
       if(len=LEX_scanutf8(&mg->lex,text,lemma))
        {
         p.fit=_strdup(lemma); 
        }
       else 
       if(iswdigit(ch))
        {
         while(iswdigit(ch))
          {
           len+=step;
           ch=utf8_towchar(text+len,&step);     
          }
         memcpy(lemma,text,len);lemma[len]=0;
         p.fit=_strdup(lemma); 
        }
       else  
       if(iswalpha(ch))
        {
         while(iswalpha(ch))
          {
           len+=step;
           ch=utf8_towchar(text+len,&step);     
          }
         memcpy(lemma,text,len);lemma[len]=0;
         p.fit=_strdup(lemma);  
        }     
       else  
       if(iswpunct(ch))
        {
         len+=step;
         memcpy(lemma,text,len);lemma[len]=0;
         p.fit=_strdup(lemma); 
        }      
       else
        {
         len+=step;
         memcpy(lemma,text,len);lemma[len]=0;
         p.fit=_strdup(lemma);          
        } 
       mgtokens_add(pieces,&p);
       text+=len; 
      }  
    }
   else
    {
    } 
  }
 return pieces->num; 
}

