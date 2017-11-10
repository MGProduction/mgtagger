mgtagger
====

This is a very simple generic pos tagger, featuring ngrams with most common word spice, with Viterbi-like code.

The source code in this repository is provided under the terms of the [Apache License, Version 2.0](http://www.apache.org/licenses/LICENSE-2.0.html).

## Information

mgtagger is able to learn the info needed to postag from inline pos tagged file (the/DT cat/NN is/VBZ on/IN the/DT table/NN) or from conllu files (in which case you can select which feature set to use, and you'll also get base forms in output.
After the quick learning phase it generates (and it's able to load) a (text) .mg file - lex + ngrams - so you can easily use different ones.
It works in utf8 - but you can switch it to codepage (changing this setting in the code)

To use it you in your code you simply need to add to your project mgtagger_postag.c + mgtagger_private.h / mgtagger.h
