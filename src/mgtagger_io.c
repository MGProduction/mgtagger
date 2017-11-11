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
// File:    mgtagger_io.c
//          This source code reads conllu and pos file
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
#include <math.h>

#include "mgtagger.h"
#include "mgtagger_private.h"

// --------------------------------------------------------------------

LEX*_feat;

// --------------------------------------------------------------------

void lemmapossplit(const char*sz,char*lemma,char*pos,char separator)
{
 int i=0,sep=-1;
 while(sz[i])
  {
   if(sz[i]==separator)
    sep=i;
   i++; 
  }  
 if(sep!=-1) 
  {
   if(lemma)
    {memcpy(lemma,sz,sep);lemma[sep]=0;}
   if(pos)
    strcpy(pos,sz+sep+1);
  }
 else
  {
   if(lemma)
    strcpy(lemma,sz);
   if(pos)
    *pos=0;
  }
}

// --------------------------------------------------------------------
// detect if _ or / is used as lemma pos separator
// --------------------------------------------------------------------
char mgtagger_getseparatorchar(const char*line)
{
 int i=0;
 while(line[i]&&(line[i]!=' '))
  if((line[i]=='_')||(line[i]=='/'))
   return line[i];
  else
   i++; 
 return '_';  
}

// --------------------------------------------------------------------
//
// --------------------------------------------------------------------
int mgtokens_readfromconllu(mgtokens*pieces,FILE*f,char*line,int linesize,int format,int flags,slex*dep)
{
 mgtoken    p;
 const char*l=line,*multi;
 char       itemid[64],fit[lemma_size],base[lemma_size],posA[128],posB[128],gram[128],ref[32],log[64];
 char       tmp[256];
 int        skip=0;
 memset(&p,0,sizeof(p));
 l=gettoken(l,itemid,'\t');
 l=gettoken(l,fit,'\t');
 if(multi=strstr(itemid,"-"))
  {
   int  ll;   
   skip=atoi(multi+1)-atoi(itemid);
   fgets(line,linesize,f);
   ll=(int)strlen(line);
   while((line[ll-1]=='\n')||(line[ll-1]=='\r'))
    line[--ll]=0;       
   l=gettoken(line,tmp,'\t');
   l=gettoken(l,tmp,'\t'); 
  }
 l=gettoken(l,base,'\t');
 l=gettoken(l,posA,'\t');
 l=gettoken(l,posB,'\t');
 l=gettoken(l,gram,'\t');
 if(flags&1)
  {           
   l=gettoken(l,ref,'\t');
   l=gettoken(l,log,'\t');
   l=gettoken(l,tmp,'\t');
   l=gettoken(l,tmp,'\t');
   
   if(strcmp(tmp,"SpaceAfter=No")==0)
    p.flags|=mgtoken_nospaceafter;
  }
 else
  *base=0;   
 
 p.fit=_strdup(fit);
 if(*base)
  p.base=_strdup(base);
 if(format==2)
  {
   const char*g=gram;
   int        l,e=1,gender=0,number=0,prontype=0,vform=0,vpers=0,vtense=0,wcase=0,wposs=0;
   strcpy(p.opos,posA);
   if((strcmp(p.opos,"DET")==0)||(strcmp(p.opos,"ADJ")==0))
    {gender=1;number=1;wcase=1;}
    if(strcmp(p.opos,"NOUN")==0)
    {gender=1;number=1;wcase=1;}
   if(strcmp(p.opos,"PRON")==0)
    {prontype=1;number=1;wcase=1;wposs=1;}
   if(strcmp(p.opos,"ADV")==0)
    prontype=1;
   if((strcmp(p.opos,"VERB")==0)||(strcmp(p.opos,"AUX")==0))
    {vform=1;vpers=1;number=1;vtense=1;}  
   l=(int)strlen(p.opos);
   while(g&&*g)
    {
     char info[96];
     g=gettoken(g,info,'|');
     if(*info)
      {
       char       attr[64];
       const char*value;
       value=gettoken(info,attr,'=');
       
       if(_feat)
        LEX_add(_feat,posA,attr,NULL,NULL);
       
       if(strcmp(attr,"Gender")==0)
        {
         if(gender)
          {
           if(e) p.opos[l++]='.'; e=0;
           p.opos[l++]=*value;
          } 
        } 
       else 
       if(strcmp(attr,"Number")==0)
        {
         if(number)
          {
           if(e) p.opos[l++]='.'; e=0;         
           p.opos[l++]=*value;
          } 
        } 
       else 
       if(strcmp(attr,"Definite")==0)
        {
         int k;
         k=0;
        }
       else
       if(strcmp(attr,"PronType")==0)
        {
         if(prontype)
          {
           if(e) p.opos[l++]='.'; e=0;         
           p.opos[l++]=*value;
          } 
        }
       else 
       if(strcmp(attr,"VerbForm")==0)
        {
         if(vform)
          {
           if(e) p.opos[l++]='.'; e=0;         
           p.opos[l++]=*value;
          } 
        }
       else 
       if(strcmp(attr,"Tense")==0)
        {
         if(vtense)
          {
           if(e) p.opos[l++]='.'; e=0;         
           p.opos[l++]=*value;
          } 
        }
       else 
       if(strcmp(attr,"Case")==0)
        {
         if(wcase)
          {
           if(e) p.opos[l++]='.'; e=0;         
           p.opos[l++]=*value;
          } 
        }
       else 
       if(strcmp(attr,"Poss")==0)
        {
         if(wposs)
          {
           if(e) p.opos[l++]='.'; e=0;         
           p.opos[l++]=*value;
          } 
        }
       else 
       if(strcmp(attr,"Person")==0)
        {
         if(vpers)
          {
           if(e) p.opos[l++]='.'; e=0;  
           if(p.opos[l-1]=='S')
            p.opos[l-1]=*value;
           else 
           if(p.opos[l-1]=='P')
            p.opos[l-1]=*value+3;
           else 
            p.opos[l++]=*value;
          } 
        }
       else 
        {
         int k;
         k=0;        
        } 
      }     
    }  
   if(strcmp(p.opos,"PUNCT")==0)
    if((p.fit[1]==0)&&ispunct(p.fit[0])&&((p.fit[0]==',')||(p.fit[0]=='.')||(p.fit[0]=='"')||(p.fit[0]=='?')||(p.fit[0]=='!')||(p.fit[0]==':')||(p.fit[0]==';')))
     {
      if(e) p.opos[l++]='.'; e=0;         
      p.opos[l++]=p.fit[0];
     }
    
   p.opos[l++]=0;   
  }
 else 
 if(format==1)
  if(strcmp(posB,"_")==0)
   strcpy(p.opos,posA); 
  else
   strcpy(p.opos,posB);
 else
  strcpy(p.opos,posA);  
 if(dep)
  { 
   p.id=atoi(itemid);
   p.dependencyid=atoi(ref);
   p.dependencykind=slex_add(dep,log); 
  } 
 mgtokens_add(pieces,&p);
 
 if(skip)
  while(skip--)
   fgets(line,linesize,f);
 
 return ((int)strlen(p.fit))+((p.flags&1)==0);
}

// --------------------------------------------------------------------
//
// --------------------------------------------------------------------
int mgtokens_readfromsequence(mgtokens*pieces,const char*line,char separator_char)
{
 int i,kbs=0;
 
 mgtokens_reset(pieces);
 
 i=0; 
 while(line[i])
  {
   char    lemmapos[lemma_size+1+pos_size],lemma[lemma_size],pos[pos_size];
   int     j=0;
   mgtoken p;
   memset(&p,0,sizeof(p));
   
   if(separator_char)
    {
     while(line[i]&&(line[i]!=separator_char))
      lemmapos[j++]=line[i++];
     lemmapos[j++]=line[i++]; 
    } 
   while(line[i]&&(line[i]!=' '))
    lemmapos[j++]=line[i++];
   lemmapos[j]=0; 
   
   lemmapossplit(lemmapos,lemma,pos,separator_char);
   p.fit=_strdup(lemma);
   strcpy(p.opos,pos);   
   mgtokens_add(pieces,&p);
   
   kbs+=(int)strlen(lemma)+1;
   
   while(line[i]==' ') i++;
   if((line[i]=='\n')||(line[i]=='\r'))
    break;           
  }          
  
 for(i=0;i<pieces->num;i++)
  pieces->pieces[i].pos=pieces->pieces[i].opos;   
 
 return kbs;
}

// --------------------------------------------------------------------
//
// --------------------------------------------------------------------
int mgtokens_readfromtext(mgtokens*pieces,const char*line)
{
 int i,kbs=0;
 
 mgtokens_reset(pieces);
 
 i=0; 
 while(line[i])
  {
   char    lemma[lemma_size];
   int     j=0;
   mgtoken p;
   memset(&p,0,sizeof(p));
   
   while(line[i]&&(line[i]!=' '))
    lemma[j++]=line[i++];
   lemma[j]=0; 
      
   p.fit=_strdup(lemma);
   mgtokens_add(pieces,&p);
   
   kbs+=(int)strlen(lemma)+1;
   
   while(line[i]==' ') i++;
   if((line[i]=='\n')||(line[i]=='\r'))
    break;           
  }          
 
 return kbs;
}
