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
// File:    mgtagger_cmd.c
//          this source code can be use to generate a command line tool to create
//          .mg (lex+ngrams) files, to use or test them, even in a interactive way
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

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "mgtagger.h"
#include "mgtagger_private.h"

// --------------------------------------------------------------------

#define compare_flag 0
//#define compare_flag 256
#if defined(_DEBUG)
int  bActivateLOG=1;
#else
int  bActivateLOG=0;
#endif

FILE*g_log;
LEX *g_err;
extern FILE*g_debug;

// --------------------------------------------------------------------

int postag(const char*input,const char*mgfile,const char*output,int format)
{
 FILE*f=fopen(input,"rb");
 if(f)
  {
   mgtagger mg;
   if(mgfile==NULL) mgfile="tagger.mg";
   printf("opening engine...\n");
   if(mgtagger_new(&mg,mgfile))
    {
     char    separator_char=0;
     int     linesize=64*1024,kbs=0,mode=mode_lemmapos_pos;
     char   *line=(char*)malloc(linesize);    
     int     timer=GetTickCount();
     mgtokens pieces;
          
     mgtokens_new(&pieces);
     printf("postagging...\n");     
     
     if(strstr(input,".conllu"))
      mode=mode_lemmapos_conllu;       
     
     if(output&&*output)
      {
       FILE*out=fopen(output,"wb+");
       while(!feof(f))
        {
         fgets(line,linesize,f);
         removeendingcrlf(line);      
         mgtagger_genericparser(&mg,line,&pieces,0);
         if(pieces.num)
          {
           int i;
           mgtagger_postag_debug(&mg,&pieces,compare_flag,g_log,g_err);
           for(i=0;i<pieces.num;i++)
            {
             if(i)
              fprintf(out," ");
             fprintf(out,"%s/%s",pieces.pieces[i].fit,pieces.pieces[i].pos);
            }
           fprintf(out,"\r\n"); 
          } 
        }  
       fclose(out); 
      }
     else 
     if(mode==mode_lemmapos_pos)
      {
       while(!feof(f))
        {
         fgets(line,linesize,f);
         removeendingcrlf(line);
          
         if(separator_char==0) separator_char=mgtagger_getseparatorchar(line); 
         
         kbs+=mgtokens_readfromsequence(&pieces,line,separator_char);
         
         if(pieces.num)
          mgtagger_postag_debug(&mg,&pieces,compare_flag,g_log,g_err);
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
            mgtagger_postag_debug(&mg,&pieces,compare_flag,g_log,g_err);
            
           mgtokens_reset(&pieces); 
          } 
         else
         if(isdigit(*line))
          kbs+=mgtokens_readfromconllu(&pieces,f,line,linesize,format,1,&mg.dependencies);
        }        
      }
     else     
      printf("file format not supported\n"); 
     
     fclose(f); 
      
     timer=GetTickCount()-timer; 
     
     if(mg.good+mg.good2+mg.err+mg.err2)
      {
       char out[256];
       sprintf(out,"POS tagged %d kbs of text in %.2f seconds (%.2f kb/sec)\r\n\r\n",kbs/1024,(float)timer/1000,(float)(kbs/1024)/((float)timer/1000));
       printf("%s\r\n",out);
       sprintf(out,"POS tagging quality : %.2f%% (good: %d err: %d)",(float)(mg.good+mg.good2)*100.0f/(float)(mg.good+mg.good2+mg.err+mg.err2),mg.good+mg.good2,mg.err+mg.err2);
       printf("%s\r\n",out);
       sprintf(out,"(know elements)     : %.2f%% (good: %d err: %d)",(float)mg.good*100.0f/(float)(mg.good+mg.err),mg.good,mg.err);
       printf("%s\r\n",out);
       sprintf(out,"(unknown elements)  : %.2f%% (good: %d err: %d)",(float)mg.good2*100.0f/(float)(mg.good2+mg.err2),mg.good2,mg.err2);       
       printf("%s\r\n",out);
       sprintf(out,"(errors for misspos): %.2f%% (errM: %d)",(float)mg.errM*100.0f/(float)(mg.err+mg.err2),mg.errM);
       printf("%s\r\n",out);
      }        
          
     free(line); 
      
     printf("closing engine...\n");
      
     mgtokens_delete(&pieces); 
     mgtagger_delete(&mg);
     
     return 1;    
    }
   else
    {
     fclose(f);
     
     printf("Sorry, can't open %s\n",mgfile); 
     return 1; 
    } 
  } 
 else
  {
   printf("Sorry, can't open %s\n",input); 
   return 0; 
  }  
}



// --------------------------------------------------------------------
// MAIN
// --------------------------------------------------------------------

int main(int argc, char* argv[])
{
 int err=0; 
 
 if((argc>2)&&((strcmp(argv[1],"learn")==0)||(strcmp(argv[1],"l")==0)))
  {
   int format=2;
   if(argc>4) 
    format=atoi(argv[4]);
   mgtagger_learn(argv[2],argv[3],format,0);
  } 
 else 
 if((argc>2)&&((strcmp(argv[1],"check")==0)||(strcmp(argv[1],"c")==0)))
  {
   int format=2; 
   LEX err;
   LEX_new(&err);
   if(argc>4) 
    format=atoi(argv[4]);   
   // --- g_debug=fopen("debug.txt","wb+"); 
   if(bActivateLOG) 
    g_log=fopen("log.txt","wb+"); 
   else
    g_log=NULL; 
   g_err=&err;   
   postag(argv[2],argv[3],NULL,format);
   if(g_log)
    {
     fprintf(g_log,"\r\n\r\n");
     LEX_write(&err,NULL,"errors",g_log,1,1);
     fclose(g_log);
    } 
   LEX_delete(&err);
   // --- fclose(g_debug);
  } 
 else 
 if((argc>3)&&(strcmp(argv[1],"b")==0))
  {
   int format=2; 
   LEX err;
   LEX_new(&err);
   if(argc>5) 
    format=atoi(argv[5]);   
   mgtagger_learn(argv[2],argv[4],format,0);
   if(bActivateLOG) 
    g_log=fopen("log.txt","wb+"); 
   else
    g_log=NULL; 
   g_err=&err;      
   postag(argv[3],argv[4],NULL,format);
   if(g_log)
    {
     fprintf(g_log,"\r\n\r\n");
     LEX_write(&err,NULL,"errors",g_log,1,1);
     fclose(g_log);
    } 
   LEX_delete(&err);
  } 
 else 
 if((argc>2)&&((strcmp(argv[1],"tag")==0)||(strcmp(argv[1],"t")==0)))
  postag(argv[2],(argc>3)?argv[4]:NULL,argv[3],0);
 else
 if((argc>1)&&(strcmp(argv[1],"i")==0))
  {   
   const char*mgfile=argv[2];
   mgtagger   mg;
   if(mgfile==NULL) mgfile="tagger.mg";
   if(mgtagger_new(&mg,mgfile))
    {
     char line[8192];
     mgtokens pieces;
     printf("opening engine...\n");
     mgtokens_new(&pieces);
     printf("write your text (use space as word separator)...\n");   
     while(gets(line))
      {
       int i;
       mgtokens_readfromsequence(&pieces,line,0);
       if(pieces.num==0)
        break;
       else
        { 
         mgtagger_postag(&mg,&pieces,compare_flag);
         for(i=0;i<pieces.num;i++)
          {
           mgtoken*piece=&pieces.pieces[i];
           if(piece->lemma&&(piece->selected!=-1))
            printf("%s/%s ",piece->fit,piece->pos);
           else
            printf("%s/UNK ",piece->fit);
          }  
         printf("\n"); 
        } 
      }
     mgtokens_delete(&pieces); 
     mgtagger_delete(&mg); 
    }  
  } 
 else
  err=1; 
  
 if(err)
  {
   printf("MG tagger - a basic POS tagger by Marco Giorgini\n");
   printf("Usage: tagger l <taggedfile> [<tagger.mg>]\n");
   printf("              LEARN - create lex file from taggedfile, generating tagger.mg\n");
   printf("              c <taggedfile> [<tagger.mg>]\n");
   printf("              CHECK - check tagger quality using generated tagger.mg\n");
   printf("              b <train> <test> [<tagger.mg>]\n");
   printf("              LEARN&CHECK - learn from train and check test quality using generated tagger.mg\n");   
   printf("              i [<tagger.mg>]\n");
   printf("              INTERACTIVE - write text and get it postagged, using generated tagger.mg\n");   
   printf("              t <inputfile> <outputfile> [<tagger.mg>]\n");
   printf("              TAG - read text input file generating postagged output file\n");
   return 1;
  } 
 else
	 return 0;
}

