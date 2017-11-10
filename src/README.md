# mgtagger source code

If you want to use mgtagger in your code you just need to add to your project mgtagger_postag.c + mgtagger_private.h / mgtagger.h

# mgtagger command line tool

*mgtagger_cmd.c* is the main source code for a command line tool to learn dictionary files from corpus, to test the quality of
this engine (if you have train/test sets), and to interactively use the tool for a quick test. 

To compile the command line project you also need mgtagger_io.c and mgtagger_learn.c (together with mgtagger_postag.c + 
mgtagger_private.h / mgtagger.h)

# usage

       mgtagger l <taggedfile> [<tagger.mg>]
                LEARN - create lex file from taggedfile, generating tagger.mg
                c <taggedfile> [<tagger.mg>]
                CHECK - check tagger quality using generated tagger.mg
                b <train> <test> [<tagger.mg>]
                LEARN&CHECK - learn from train and check test quality using generated tagger.mg
                i [<tagger.mg>]
                INTERACTIVE - write text and get it postagged, using generated tagger.mg
                t <inputfile> <outputfile> [<tagger.mg>]
                TAG - read text input file generating postagged output file
