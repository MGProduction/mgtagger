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
// File:    mgtagger_learn.c
//          This source code autolearn info from a postagged file
//          It's not needed if you just want to use the tagger in your own code, to postag.
//
// This is a very simple generic pos tagger, featuring ngrams with most common word spice, with Viterbi-like code.
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

#include "mgtagger.h"
#include "mgtagger_private.h"

extern LEX*_feat;

int INWARDS_reduce(NGRAMS*ngrams,NGRAMS*inward,int cut,int dir)
{
 int i,del=0;
 for(i=0;i<inward->num;i++)
  {
   NGRAM     *fnd,*sfnd,*sfndI;
   char       tpos[256],slice[256];
   const char*inw=inward->ngrams[i].ngram;
   int        inwcnt=inward->ngrams[i].cnt;
   int        j=0,k=0,cnt=1,jj=-1,kk=-1,ljj=-1,lkk=-1;
   if(inwcnt<cut)
    continue;
   while(inw[j])
    if((inw[j]==':')&&j&&(inw[j-1]!=' ')&&(inw[j+1]!=' ')&&(inw[j+1]!=0))
     {
      while(inw[j]&&(inw[j]!=' '))
       j++;
     }
    else
     {
      if(inw[j]==' ') 
       {
        cnt++;
        if(jj==-1) jj=j;
        if(kk==-1) kk=k;
        ljj=j;
        lkk=k;
       } 
      tpos[k++]=inw[j++]; 
     } 
   tpos[k]=0;
   if(cnt>1)
    {
     if(dir==0)
      {strcpy(slice,inw);slice[ljj]=0;}
     else
      strcpy(slice,inw+jj+1); 
     sfndI=NGRAMS_find(inward,slice);
     
     fnd=NGRAMS_find(&ngrams[cnt-1],tpos);
     if(dir==0)
      {strcpy(slice,tpos);slice[lkk]=0;}
     else 
      strcpy(slice,tpos+kk+1);
     sfnd=NGRAMS_find(&ngrams[cnt-2],slice);
     
     if(((float)inwcnt/(float)sfndI->cnt)<((float)fnd->cnt/(float)sfnd->cnt))
      {inward->ngrams[i].cnt=0;del++;}
    } 
  }
 return del; 
}

void NGRAMS_reduce(NGRAMS*l,int limit)
{
 int i;
 if(limit==-1)
  {
   int sum=0,avg=0;
   for(i=0;i<l->num;i++)
    sum+=l->ngrams[i].cnt;
   avg=sum/l->num;
   limit=avg/10;
   if(limit<1)
    limit=1;
  }
 qsort(l->ngrams,l->num,sizeof(l->ngrams[0]),NGRAM_compare);
 for(i=0;i<l->num;i++)
  {
   if(l->ngrams[i].cnt<=limit)
    {
     l->num=i;
     break;
    }
  }  
}

int NGRAMS_write(FILE*f,NGRAMS*inward,NGRAMS*ngrams,int ngramscnt)
{ 
 if(f)
  {
   int    i,j; 
   NGRAMS*l;  
      
   for(j=0;j<max_inwards;j++)
    {
     l=inward+j;  
     if(l->num)
      {
       qsort(l->ngrams,l->num,sizeof(l->ngrams[0]),NGRAM_comparesz);   
       fprintf(f,"[INWARDS:%d]\r\n%d\r\n",j+1,l->num);
       for(i=0;i<l->num;i++)
        fprintf(f,"%s\t%d\r\n",l->ngrams[i].ngram,l->ngrams[i].cnt);
       fprintf(f,"\r\n"); 
      } 
    } 
   
   for(j=0;j<ngramscnt;j++)
    {
     l=ngrams+j;  
     if(l->num)
      {
       qsort(l->ngrams,l->num,sizeof(l->ngrams[0]),NGRAM_comparesz);   
       fprintf(f,"[NGRAMS:%d]\r\n%d\r\n",j+1,l->num);
       for(i=0;i<l->num;i++)
        fprintf(f,"%s\t%d\r\n",l->ngrams[i].ngram,l->ngrams[i].cnt);
       fprintf(f,"\r\n"); 
      }
     else
      break;  
    } 
            
   return 1; 
  } 
 else
  return 0; 
}

// --------------------------------------------------------------------

void LEX_sortbycountrev(LEX*l)
{
 qsort(l->lemmas,l->num,sizeof(l->lemmas[0]),LEMMACNT_compare);
}

int ULEX_reduce(LEX*l)
{
 int i,ll,del=0,mlen=256,Mlen=0;
 LEX_sortbystring(l);
 for(i=0;i<l->num;i++)
  {
   LEMMA     *cur=&l->lemmas[i];
   const char*unk=cur->lemma;
   const char*p=strstr(unk,"*");
   int        len=(int)strlen(unk);
   if(len>Mlen)
    Mlen=len;
   if(len<mlen)
    mlen=len; 
  } 
 for(ll=Mlen;ll>mlen;ll--) 
  for(i=0;i<l->num;i++)
   {
    LEMMA*cur=&l->lemmas[i];
    const char*unk=cur->lemma,*p=strstr(unk,"*");
    int        len=(int)strlen(unk);
    if(len==ll)
     if(p&&(strlen(p+1)>2))
      {
       char  red[256];
       LEMMA*fnd;
       memcpy(red,unk,(p-unk)+1);strcpy(red+(p-unk)+1,p+2);
       fnd=LEX_find(l,red);
       if(fnd)
        {
         if(fnd->num==cur->num)
          {
           int k,cnt;
           for(cnt=k=0;k<fnd->num;k++)
            if(strcmp(fnd->poss[k].pos,cur->poss[k].pos)==0)
             cnt++;
           if(cnt==fnd->num)
            {
             cur->cnt=0;
             del++;  
            } 
          }
        }
      } 
   }
 return del;  
}

int LEX_write(LEX*l,LEX*ulex,const char*name,FILE*f,int way,int utf8)
{
 if(f)
  {
   int i,j;   
   if(way==0)
    qsort(l->lemmas,l->num,sizeof(l->lemmas[0]),LEMMA_compare);
   else 
    qsort(l->lemmas,l->num,sizeof(l->lemmas[0]),LEMMACNT_compare);

   fprintf(f,"[LEX:%s]\r\n",name);
   for(i=0;i<l->num;i++)
    {
     LEMMA*w=&l->lemmas[i];
     if(ulex&&(w->num==1))
      {
       LEMMA*unk=mgtagger_getunknownlemma(ulex,w->lemma,utf8,-1);
       if(unk&&(unk->num==1)&&(strcmp(unk->poss[0].pos,w->poss[0].pos)==0))
        continue;
      }
     fprintf(f,"%s\t%d",w->lemma,w->cnt);
     if(w->num>1)
      qsort(w->poss,w->num,sizeof(w->poss[0]),POS_compare);
     for(j=0;j<w->num;j++) 
      if(w->poss[j].cnt<=(w->poss[0].cnt/1000))
       break;
      else 
       {
        int cnt=w->poss[j].cnt;
        if(w->poss[j].base)
         fprintf(f," %s_%s %d",w->poss[j].pos,w->poss[j].base,cnt);
        else
         fprintf(f," %s %d",w->poss[j].pos,cnt);
       }  
     if(w->flags&1) fprintf(f," ***");
     fprintf(f,"\r\n");
    }
   fprintf(f,"\r\n"); 
   return 1;
  } 
 else
  return 0; 
}

// --------------------------------------------------------------------

char       mgtagger_getseparatorchar(const char*line);

// --------------------------------------------------------------------

int mgtagger_write(mgtagger*mg,const char*savename)
{
 if(savename&&*savename)
  {
   FILE*f=fopen(savename,"wb+");
   if(f)
    {
     fprintf(f,"# ----------------------------\r\n");
     fprintf(f,"# -\r\n");
     fprintf(f,"# - MGTAGGER resource file\r\n");
     fprintf(f,"# -\r\n");
     fprintf(f,"# ----------------------------\r\n");
     fprintf(f,"$version=1\r\n");
     if(mg->utf8)
      fprintf(f,"$codepage=utf-8\r\n");
     else 
      fprintf(f,"$codepage=ansi\r\n");
     fprintf(f,"\r\n");
     LEX_sortbystring(&mg->ulex);
     LEX_write(&mg->lex,&mg->ulex,"main",f,0,mg->utf8);
     LEX_write(&mg->colls,&mg->colls,"colls",f,0,mg->utf8);
     LEX_write(&mg->ulex,NULL,"unknown",f,0,mg->utf8);
     LEX_write(&mg->deprules,NULL,"dependencies",f,0,mg->utf8);
     NGRAMS_write(f,&mg->inward[0],&mg->ngrams[0],max_ngrams);
     fclose(f);
    }
  }
 return 1;
}

// --------------------------------------------------------------------

typedef struct {
 const char*lemma;
 int        lemmaID;
}hashlemma;

typedef struct {
 int       num,size;
 hashlemma*items;
}hashlemmas;

unsigned int hashfunct(const char*str)
{
 unsigned long hash = 5381;
 int           c;
 while (c = (unsigned char)*str++)
   hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
 return hash;
}

void hashlemmas_new(hashlemmas*h,int size)
{
 h->num=0;
 h->size=size/*8803*/;
 h->items=(hashlemma*)calloc(h->size,sizeof(h->items[0]));
}

void hashlemmas_delete(hashlemmas*h)
{
 free(h->items);
}

hashlemma*hashlemmas_find(hashlemmas*h,const char*lemma)
{
 unsigned int i=hashfunct(lemma)%h->size;
 while(h->items[i].lemma!=NULL)
  if(strcmp(h->items[i].lemma,lemma)==0)
   return &h->items[i];
  else 
   i=(i+1)%h->size;
 return NULL;  
}

void hashlemmas_add(hashlemmas*h,const char*lemma,int lemmaID)
{
 unsigned int i=hashfunct(lemma)%h->size;
 while(h->items[i].lemma!=NULL)
  if(strcmp(h->items[i].lemma,lemma)==0) 
   return;
  else
   i=(i+1)%h->size;
 
 if(h->items[i].lemma==NULL) 
  {
   h->num++;
   if(h->num>h->size*85/100)
    {
     int        i;
     hashlemmas nh;
     hashlemmas_new(&nh,h->size*3-17);
     for(i=0;i<h->size;i++)
      if(h->items[i].lemma)
       hashlemmas_add(&nh,h->items[i].lemma,h->items[i].lemmaID);
     hashlemmas_add(&nh,lemma,lemmaID);  
     hashlemmas_delete(h);
     h->items=nh.items;
     h->num=nh.num;
     h->size=nh.size;
    }
   else
    { 
     h->items[i].lemma=lemma;
     h->items[i].lemmaID=lemmaID;
    } 
  }
}

// --------------------------------------------------------------------

typedef struct NGRAMS_TREE_ITEM;
typedef struct{
 NGRAM            here;
 struct NGRAMSTREE_ITEM *next;
 struct NGRAMSTREE_ITEM*child;
}NGRAMSTREE_ITEM;

typedef struct {
 NGRAMSTREE_ITEM*root;
}NGRAMSTREE;

void NGRAMSTREE_new(NGRAMSTREE*t)
{
 t->root=NULL;
}

void NGRAMSTREE_ITEM_delete(NGRAMSTREE_ITEM*n)
{
 if(n->child) NGRAMSTREE_ITEM_delete((NGRAMSTREE_ITEM*)n->child);
 if(n->next) NGRAMSTREE_ITEM_delete((NGRAMSTREE_ITEM*)n->next);
 free(n);
}

void NGRAMSTREE_toNGRAMS(NGRAMSTREE_ITEM*t,NGRAMS*out,char*seq,int deep,int flags)
{ 
 int len=(int)strlen(seq);
 while(t)
  {
   if(deep)
    {
     seq[len]=' ';
     strcpy(seq+len+1,t->here.ngram);    
    }
   else
    strcpy(seq+len,t->here.ngram);    
   if((deep==0)||(t->here.cnt>1))
    if(flags&1)
     NGRAMS_addex(&out[deep],seq,t->here.cnt,1);
    else
     NGRAMS_addex(out,seq,t->here.cnt,1); 
   if(t->child)
    NGRAMSTREE_toNGRAMS((NGRAMSTREE_ITEM*)t->child,out,seq,deep+1,flags);
   t=(NGRAMSTREE_ITEM*)t->next;
  }
}

void NGRAMSTREE_delete(NGRAMSTREE*t)
{
 NGRAMSTREE_ITEM_delete(t->root);
}

void NGRAMSTREE_ITEM_add(NGRAMSTREE_ITEM*n,const char**tag,int p,int t)
{
 while(p<t)
  {
   if(n->here.ngram==NULL)
    n->here.ngram=(char*)tag[p];
   if(strcmp(n->here.ngram,tag[p])==0)
    {     
     if(p+1==t)
      {
       n->here.cnt++;
       return;
      } 
     else
      {
       if(n->child==NULL) n->child=(struct NGRAMSTREE_ITEM*)calloc(1,sizeof(NGRAMSTREE_ITEM));
       NGRAMSTREE_ITEM_add((NGRAMSTREE_ITEM*)n->child,tag,p+1,t);
       return;
      } 
    }
   if(n->next==NULL) n->next=(struct NGRAMSTREE_ITEM*)calloc(1,sizeof(NGRAMSTREE_ITEM));
   n=(NGRAMSTREE_ITEM*)n->next; 
  }
}

void NGRAMSTREE_add(NGRAMSTREE*t,const char**seq,int mt)
{
 if(t->root==NULL) t->root=(NGRAMSTREE_ITEM*)calloc(1,sizeof(NGRAMSTREE_ITEM));
 NGRAMSTREE_ITEM_add(t->root,seq,0,mt);
}

// --------------------------------------------------------------------
// MACHINE LEARNING code
// --------------------------------------------------------------------

void mgtagger_learn_lex(mgtagger*mg,mgtokens*pieces,hashlemmas*hl)
{
 int i;
   
 for(i=-1;i<pieces->num+1;i++)
  {
   const char*curlemma=mgtokens_getlemma(pieces,i);
   const char*curpos=mgtokens_getpos(pieces,i);
   const char*curbase=mgtokens_getbase(pieces,i);
   const char*curgrammar=mgtokens_getgrammar(pieces,i);
   if(hl)
    {     
     hashlemma* cur=hashlemmas_find(hl,curlemma);
     
     if(cur==NULL)
      {
       int curID=LEX_add(&mg->lex,curlemma,curpos,curbase,curgrammar);                    
       hashlemmas_add(hl,mg->lex.lemmas[curID].lemma,curID);
      } 
     else  
      LEX_addpos(&mg->lex,cur->lemmaID,curpos,curbase,curgrammar,1);
    }
   else   
    LEX_add(&mg->lex,curlemma,curpos,mgtokens_getbase(pieces,i),curgrammar);                    
   
  }
}

void mgtagger_learn_prepare_pos(mgtagger*mg,mgtokens*pieces)
{
 int i;
 for(i=0;i<pieces->num;i++)
  pieces->pieces[i].pos=pieces->pieces[i].opos; 
}

void mgtagger_learn_dependencies(mgtagger*mg,mgtokens*pieces)
{
 int i,gcnt=0,dontcheckdistance=0,roots=0;
 for(i=0;i<pieces->num;i++)
  {
   mgtoken*piece=&pieces->pieces[i];
   if(*piece->dependencykind==0)
    piece->dependencykind=NULL;
   if(piece->dependencykind)
    {
     if(piece->dependencyid==0)
      roots++;
     gcnt++;
    } 
  }
  
 if(gcnt&&(mg->deprules.size==0))
  LEX_new(&mg->deprules);
 
 while(gcnt>0)
  {
   int pgcnt=gcnt;
   for(i=0;i<pieces->num;i++)
    {
     mgtoken*piece=&pieces->pieces[i];
     if(piece->dependencykind)
      {
       if(piece->dependencyid==0)
        {
         if(gcnt<=roots)
          {
           LEX_add(&mg->deprules,piece->pos,piece->dependencykind,NULL,NULL);
           gcnt--;
           piece->dependencykind=NULL;
          } 
        } 
       else 
       if(piece->dependencyid>piece->id)
        {
         int cnt=0,j=i+1,fj=-1;
         while((j<pieces->num)&&(pieces->pieces[j].id<=piece->dependencyid))
          if(pieces->pieces[j].id==piece->dependencyid)
           {
            cnt++;fj=j;
            break;
           }
          else
           {
            if(pieces->pieces[j].dependencykind)
             {cnt++;fj=j;}
            j++;
           } 
         if(cnt==0)
          {
           piece->dependencykind=NULL;
           gcnt--;
          }
         else              
         if((cnt==1)||dontcheckdistance)
          {
           char tpos[256];
           strcpy(tpos,piece->pos);strcat(tpos,">");strcat(tpos,pieces->pieces[fj].pos);
           LEX_add(&mg->deprules,tpos,piece->dependencykind,NULL,NULL);
           piece->dependencykind=NULL;
           gcnt--;
          }  
        }
       else 
       if(piece->dependencyid<piece->id)
        {
         int cnt=0,j=i-1,fj=-1;
         while((j>=0)&&(pieces->pieces[j].id>=piece->dependencyid))
          if(pieces->pieces[j].id==piece->dependencyid)
           {
            cnt++;fj=j;
            break;
           } 
          else
           {
            if(pieces->pieces[j].dependencykind)
             {cnt++;fj=j;}
            j--;
           } 
         if(cnt==0)
          {
           piece->dependencykind=NULL;
           gcnt--;
          }
         else   
         if((cnt==1)||dontcheckdistance)
          {
           char tpos[256];
           strcpy(tpos,piece->pos);strcat(tpos,"<");strcat(tpos,pieces->pieces[fj].pos);
           LEX_add(&mg->deprules,tpos,piece->dependencykind,NULL,NULL);
           piece->dependencykind=NULL;
           gcnt--;
          }
        }     
      }
    }
   if(pgcnt==gcnt)
    dontcheckdistance=1; 
  }    
}

void mgtagger_learn_ngrams(mgtagger*mg,mgtokens*pieces,int hicut,NGRAMSTREE*tree)
{
 int i;
  
 for(i=-1;i<pieces->num+1;i++)
  {
   const char*tpos[256];
   int        r;
   const char*curlemma=mgtokens_getlemma(pieces,i);
   const char*curpos=mgtokens_getpos(pieces,i);
   const char*curbase=mgtokens_getbase(pieces,i);   
   
   r=max_ngrams;
   while(r)
    {
     r--;     
     if(i-r>=-1)
      {
       int d=r+1,t=0;
       while(d&&(t<sizeof(tpos)/sizeof(tpos[0])))
        {
         d--;
         tpos[t++]=slex_add(&mg->pos,mgtokens_getpos(pieces,i-d));
        }
       if(t)
        NGRAMSTREE_add(tree,tpos,t);
      }
    }       
  }
}

void mgtagger_learn_inwards(mgtagger*mg,mgtokens*pieces,int width,int hicut,NGRAMSTREE*tree,hashlemmas*hlc)
{
 int i;

 for(i=0;i<pieces->num;i++)
  {
   LEMMA*plemma=LEX_find(&mg->lex,mgtokens_getlemma(pieces,i));
   pieces->pieces[i].lemma=plemma;
   if(plemma)
    if(plemma->flags&lemma_hifreq)
     {
      int        j; 
      const char*tpos[256];
      char       rich[256];
      const char*cpos=mgtokens_getpos(pieces,i);
      for(j=0;j<plemma->num;j++)
       if(strcmp(plemma->poss[j].pos,cpos)==0)
        {
         if(plemma->poss[j].cnt>hicut)
          {
           int        l,j;
           for(l=0;l<width;l++)            
            {             
             if(i-l>=-1)
              {
               int t=0;
               for(j=0;j<l;j++)
                tpos[t++]=slex_add(&mg->pos,mgtokens_getpos(pieces,i-l+j));
               sprintf(rich,"%s:%s",mgtokens_getpos(pieces,i),mgtokens_getlemma(pieces,i)); 
               tpos[t++]=slex_add(&mg->pos,rich);
               NGRAMSTREE_add(tree+1,tpos,t);
              }        
             else
              break;     
            }           
           for(l=0;l<width;l++)            
            { 
             if(i+l<pieces->num)
              {
               int t=0;
               sprintf(rich,"%s:%s",mgtokens_getpos(pieces,i),mgtokens_getlemma(pieces,i));
               tpos[t++]=slex_add(&mg->pos,rich);
               for(j=0;j<l;j++)
                tpos[t++]=slex_add(&mg->pos,mgtokens_getpos(pieces,i+1+j));
               NGRAMSTREE_add(tree+0,tpos,t);
              } 
            }
         }   
        break; 
       }  
     } 
   
   if((i>0)&&hlc)
    if(pieces->pieces[i-1].lemma&&(pieces->pieces[i-1].lemma->flags&lemma_hifreq)&&pieces->pieces[i].lemma&&(pieces->pieces[i].lemma->flags&lemma_hifreq))
     {
      char       lm[512];
      hashlemma* cur;      
      sprintf(lm,"%s:%s %s:%s",mgtokens_getlemma(pieces,i-1),mgtokens_getpos(pieces,i-1),mgtokens_getlemma(pieces,i),mgtokens_getpos(pieces,i));
      cur=hashlemmas_find(hlc,lm);
      if(cur==NULL)
       {
        int curID=LEX_add(&mg->colls,lm,mgtokens_getpos(pieces,i),NULL,NULL);                    
        hashlemmas_add(hlc,mg->colls.lemmas[curID].lemma,curID);
       } 
      else  
       LEX_addpos(&mg->colls,cur->lemmaID,mgtokens_getpos(pieces,i),NULL,NULL,1);
     }            
  }
}

int mgtagger_generate_unknownlex(mgtagger*mg,int*plowcut)
{
 int hicut=1000;
 int lowcut=0;
 if(mg->lex.num)
  {
   int i,rare=2,avg,sum=0,topcnt=0;
   LEX_sortbycountrev(&mg->lex);
   
   lowcut=mg->lex.lemmas[mg->lex.num/3].cnt;
   if(mg->lex.num>1024)    
    hicut=mg->lex.lemmas[128].cnt;
   else
    hicut=mg->lex.lemmas[mg->lex.num/10].cnt; 

   for(i=0;i<mg->lex.num;i++)
    sum+=mg->lex.lemmas[i].cnt; 
   avg=sum/mg->lex.num;

   for(i=0;i<mg->lex.num;i++)      
    {
     int j;
     for(j=0;j<mg->lex.lemmas[i].num;j++)
      {
       char       ulemmaC[256];
       const char*tlemma=mg->lex.lemmas[i].lemma;
       const char*tpos=mg->lex.lemmas[i].poss[j].pos;
       const char*base=NULL;
       const char*gram=NULL;
       int        tcnt=mg->lex.lemmas[i].poss[j].cnt;
       
       if(mg->lex.lemmas[i].cnt>hicut) 
        {
         if(mg->utf8&&iswpunct(utf8_towchar(tlemma,NULL)))
          ;
         else 
         if((mg->utf8==0)&&ispunct(*tlemma))
          ;
         else 
         if(mg->lex.lemmas[i].poss[0].cnt>hicut)
          if(memcmp(mg->lex.lemmas[i].poss[0].pos,"PUNCT",5)==0)
           ;
          else 
           mg->lex.lemmas[i].flags|=lemma_hifreq;
        }
                
       if((i>mg->lex.num/8)&&(i<mg->lex.num*7/8))
        if(mg->lex.lemmas[i].cnt>=lowcut)
         {
          int l;
          for(l=0;l<=8;l++)
           if(LEX_known2unknown(tlemma,ulemmaC,l,mg->utf8))
            LEX_addex(&mg->ulex,ulemmaC,tpos,base,gram,tcnt,0);
         }  
      
      } 
    }     
  }
 if(plowcut)
  *plowcut=lowcut;  
 return hicut; 
}

// --------------------------------------------------------------------

int mgtagger_learn(const char*input,const char*mgfile,int format,int skip)
{
 FILE*f=fopen(input,"rb");
 if(f)
  {
   int        mode=mode_lemmapos_pos;
   int        linesize=64*1024;
   char      *line=(char*)malloc(linesize);  
   int        i,hicut,lowcut,poscnt=0;
   char       separator_char=0;
   int        clauses=0,maxclauses=0x7FFFFFFF;
   mgtokens   pieces;
   mgtagger   mg;
   hashlemmas hl,hlc,*phl=&hl,*phlc=&hlc;
   NGRAMSTREE tree,itree[2];
   NGRAMSTREE_new(&tree);
   for(i=0;i<2;i++)
    NGRAMSTREE_new(&itree[i]);
      
   mgtagger_new(&mg,NULL);   
   hashlemmas_new(&hl,8803);
   hashlemmas_new(&hlc,8803);
   mgtokens_new(&pieces);
   
   if(strstr(input,".conllu"))
    mode=mode_lemmapos_conllu;
   
   printf("reading postagged source file...\n");   
      
   clauses=0;
   if(mode==mode_lemmapos_pos)
    {
     while(!feof(f))
      {
       fgets(line,linesize,f);
       removeendingcrlf(line);
        
       if(separator_char==0) separator_char=mgtagger_getseparatorchar(line);
       
       mgtokens_readfromsequence(&pieces,line,separator_char);
       
       mgtagger_learn_prepare_pos(&mg,&pieces);
       mgtagger_learn_lex(&mg,&pieces,phl);
                       
       clauses++;poscnt+=pieces.num;
       if((clauses%256)==0) 
        printf("pos: %d clause: %d lex: %d            \r",poscnt,clauses,mg.lex.num);
      }
    }  
   else
   if(mode==mode_lemmapos_conllu)
    {
     while(!feof(f))
      {
       fgets(line,linesize,f);
       removeendingcrlf(line);
       if(*line==0)
        {
         if(pieces.num)
          {
           mgtagger_learn_prepare_pos(&mg,&pieces);
           
           mgtagger_learn_lex(&mg,&pieces,phl);
           mgtagger_learn_dependencies(&mg,&pieces);
           
           clauses++;poscnt+=pieces.num;
           if((clauses%256)==0) 
            printf("pos: %d clause: %d lex: %d            \r",poscnt,clauses,mg.lex.num);
          } 
         mgtokens_reset(&pieces); 
        } 
       else
       if(isdigit(*line))
        mgtokens_readfromconllu(&pieces,f,line,linesize,format,1,&mg.dependencies);
      }  
    } 
    
   hashlemmas_delete(&hl); 
   if(1)
    {
     int i,j,poscnt=0,baseforms=mg.lex.base.num;
     for(i=0;i<mg.lex.num;i++)
      {
       poscnt+=mg.lex.lemmas[i].cnt;
       for(j=0;j<mg.lex.lemmas[i].num;j++)
        if(mg.lex.lemmas[i].poss[j].base==NULL)
         {
          baseforms++;
          break;
         }
      } 
     printf("LEX has %d entries with %d baseforms, for %d hits\n",mg.lex.num,baseforms,poscnt); 
    }
   else
    printf("\n");
   
   printf("generating unknown lemmas info...\n");           
   hicut=mgtagger_generate_unknownlex(&mg,&lowcut);
   LEX_sortbystring(&mg.lex);       
   printf("generating inwords info & ngrams...\n");

   fseek(f,0,0);
   clauses=0;
   if(mode==mode_lemmapos_pos)
    {   
     while(!feof(f))
      {
       fgets(line,linesize,f);
       removeendingcrlf(line);
        
       if(separator_char==0) separator_char=mgtagger_getseparatorchar(line);
       
       mgtokens_readfromsequence(&pieces,line,separator_char);
       
       mgtagger_learn_prepare_pos(&mg,&pieces);
       
       mgtagger_learn_ngrams(&mg,&pieces,hicut,&tree);
       mgtagger_learn_inwards(&mg,&pieces,max_inwards_width,hicut,&itree[0],phlc);
                       
       clauses++;
       if((clauses%256)==0) 
        printf("clause: %d lex: %d            \r",clauses,mg.lex.num);
      }
    }  
   else
   if(mode==mode_lemmapos_conllu) 
    {
     while(!feof(f))
      {
       fgets(line,linesize,f);
       removeendingcrlf(line);
       if(*line==0)
        {
         if(pieces.num)
          {
           mgtagger_learn_prepare_pos(&mg,&pieces);
           
           mgtagger_learn_ngrams(&mg,&pieces,hicut,&tree);
           mgtagger_learn_inwards(&mg,&pieces,max_inwards_width,hicut,&itree[0],phlc);
           
           clauses++;
           if((clauses%256)==0) 
            printf("clause: %d lex: %d            \r",clauses,mg.lex.num);
          } 
         mgtokens_reset(&pieces); 
        } 
       else
       if(isdigit(*line))
        mgtokens_readfromconllu(&pieces,f,line,linesize,format,1,&mg.dependencies);
      }
    }    
   hashlemmas_delete(&hlc);   
   printf("\n");      
   
   printf("checking & reducing ngrams...\n"); 
   if(tree.root)
    {
     char seq[256];
     *seq=0; 
     NGRAMSTREE_toNGRAMS(tree.root,&mg.ngrams[0],seq,0,1);
    } 
   NGRAMSTREE_delete(&tree); 
   
   for(i=0;i<max_inwards;i++)
    {
     if(itree[i].root)
      {
       char seq[256];
       *seq=0; 
       NGRAMSTREE_toNGRAMS(itree[i].root,&mg.inward[i],seq,0,0);
      } 
     NGRAMSTREE_delete(&itree[i]); 
    } 
   
   for(i=0;i<max_ngrams;i++) 
    qsort(mg.ngrams[i].ngrams,mg.ngrams[i].num,sizeof(mg.ngrams[i].ngrams[0]),NGRAM_comparesz);    
   for(i=0;i<max_inwards;i++)
    {
     qsort(mg.inward[i].ngrams,mg.inward[i].num,sizeof(mg.inward[i].ngrams[0]),NGRAM_comparesz);
     INWARDS_reduce(&mg.ngrams[0],&mg.inward[i],lowcut,i);
     NGRAMS_reduce(&mg.inward[i],lowcut); 
    } 
   if(mg.ngrams[2].num)
    {
     int i;
     for(i=2;i<max_ngrams;i++)
      NGRAMS_reduce(&mg.ngrams[i],lowcut+(i-2)*2);
    }         
   
   fclose(f);   
   
   mgtokens_delete(&pieces);           
      
   printf("reducing LEX files...\n");   
   ULEX_reduce(&mg.ulex);
   if(mg.ulex.num>1024)
    LEX_reduce(&mg.ulex,-2);   
   else
    LEX_reduce(&mg.ulex,-1);    
   LEX_reduce(&mg.colls,lowcut*2);   
   LEX_reduce(&mg.deprules,lowcut);   
   
   printf("writing .mg file...\n");
     
   if(mgfile&&*mgfile)
    mgtagger_write(&mg,mgfile);
   else
    mgtagger_write(&mg,"tagger.mg");
   
   mgtagger_delete(&mg);
      
   free(line);
  }
 return 0;
}
