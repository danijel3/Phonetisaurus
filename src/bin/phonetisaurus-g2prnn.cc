#include <fst/fstlib.h>
using namespace std;
#include <include/LegacyRnnLMHash.h>
#include <include/LegacyRnnLMDecodable.h>
#include <include/LegacyRnnLMReader.h>
#include <include/RnnLMDecoder.h>
#include <include/util.h>
#include "utf8.h"
#ifdef _OPENMP
#include <omp.h>
#endif
using namespace fst;

typedef LegacyRnnLMDecodable<Token, LegacyRnnLMHash> Decodable;
typedef unordered_map<int, SimpleResult> RMAP;


void ThreadedEvaluateWordlist (vector<string>& corpus, RMAP& rmap,
			       LegacyRnnLMHash& h, Decodable& s, 
			       int FST_FLAGS_threads, int FST_FLAGS_beam, 
			       int FST_FLAGS_kmax, int FST_FLAGS_nbest, 
			       bool FST_FLAGS_reverse, string FST_FLAGS_gpdelim,
			       string FST_FLAGS_gdelim, string FST_FLAGS_skip,
			       double FST_FLAGS_thresh, string FST_FLAGS_gsep) {
  int csize = corpus.size ();

#ifdef _OPENMP
#pragma omp parallel for
#endif
  for (int x = 0; x < FST_FLAGS_threads; x++) {
    RnnLMDecoder<Decodable> decoder (s);

    int start = x * (csize / FST_FLAGS_threads);
    int end   = (x == FST_FLAGS_threads - 1) ? csize \
      : start + (csize / FST_FLAGS_threads);
    for (int i = start; i < end; i++) {
      vector<string> graphemes = tokenize_utf8_string (&corpus [i],
							 &FST_FLAGS_gsep);
      if (FST_FLAGS_reverse == true)
	reverse (graphemes.begin (), graphemes.end ());

      graphemes.push_back ("</s>");
      SimpleResult result = \
	decoder.Decode (graphemes, FST_FLAGS_beam, FST_FLAGS_kmax, 
			FST_FLAGS_nbest, FST_FLAGS_thresh, FST_FLAGS_gpdelim,
			FST_FLAGS_gdelim, FST_FLAGS_skip);
      rmap [i] = result;
    }
  }

  for (int i = 0; i < csize; i++) {
    const SimpleResult& result = rmap [i];

    for (int k = 0; k < result.pronunciations.size (); k++)
      cout << result.word << "\t" << result.scores [k] << "\t" 
	   << result.pronunciations [k] << "\n";
  }
}

void EvaluateWordlist (vector<string>& corpus,
		       LegacyRnnLMHash& h, Decodable& s, int FST_FLAGS_beam, 
		       int FST_FLAGS_kmax, int FST_FLAGS_nbest, bool FST_FLAGS_reverse, 
		       string FST_FLAGS_gpdelim, string FST_FLAGS_gdelim, 
		       string FST_FLAGS_skip, double FST_FLAGS_thresh,
		       string FST_FLAGS_gsep) {

  RnnLMDecoder<Decodable> decoder (s);
  for (int i = 0; i < corpus.size (); i++) {
    vector<string> graphemes = tokenize_utf8_string (&corpus [i],
						     &FST_FLAGS_gsep);
    if (FST_FLAGS_reverse == true)
	reverse (graphemes.begin (), graphemes.end ());

    graphemes.push_back ("</s>");
    
    SimpleResult result = \
      decoder.Decode (graphemes, FST_FLAGS_beam, FST_FLAGS_kmax, 
		      FST_FLAGS_nbest, FST_FLAGS_thresh, FST_FLAGS_gpdelim,
		      FST_FLAGS_gdelim, FST_FLAGS_skip);
    
    for (int k = 0; k < result.pronunciations.size (); k++)
      cout << result.word << "\t" << result.scores [k] << "\t" 
	   << result.pronunciations [k] << "\n";
  }
}

void EvaluateWord (string word, LegacyRnnLMHash& h, Decodable& s, 
		   int FST_FLAGS_beam, int FST_FLAGS_kmax, int FST_FLAGS_nbest, 
		   bool FST_FLAGS_reverse, string FST_FLAGS_gpdelim, 
		   string FST_FLAGS_gdelim, string FST_FLAGS_skip, 
		   double FST_FLAGS_thresh, string FST_FLAGS_gsep) {

  vector<string> graphemes = tokenize_utf8_string (&word,
						   &FST_FLAGS_gsep);
  if (FST_FLAGS_reverse == true)
    reverse (graphemes.begin (), graphemes.end ());
  graphemes.push_back ("</s>");
  
  RnnLMDecoder<Decodable> decoder (s);
  SimpleResult result =	\
      decoder.Decode (graphemes, FST_FLAGS_beam, FST_FLAGS_kmax, 
		      FST_FLAGS_nbest, FST_FLAGS_thresh, FST_FLAGS_gpdelim,
		      FST_FLAGS_gdelim, FST_FLAGS_skip);
    
  for (int k = 0; k < result.pronunciations.size (); k++)
    cout << result.word << "\t" << result.scores [k] << "\t" 
	 << result.pronunciations [k] << "\n";
}

DEFINE_string (rnnlm, "", "The input RnnLM model.");
DEFINE_string (wordlist, "", "Input word list to evaluate.");
DEFINE_string (word, "", "Single input word to evaluate.");
DEFINE_string (gdelim, "|", "The default multigram delimiter.");
DEFINE_string (gpdelim, "}", "The default grapheme / phoneme delimiter.");
DEFINE_string (gsep, "", "The default grapheme delimiter for testing.  Typically ''.");
DEFINE_string (skip, "_", "The default null/skip token.");
DEFINE_int32  (nbest, 1, "Maximum number of hypotheses to return.");
DEFINE_int32  (threads, 1, "Number of parallel threads (OpenMP).");
DEFINE_int32  (kmax, 20, "State-local maximum queue size.");
DEFINE_int32  (beam, 20, "The state-local beam width.");
DEFINE_double (thresh, 0.0, "The n-best pruning threshold. Relative to 1-best.");
DEFINE_bool   (reverse, false, "Reverse the input word before decoding.");

int main (int argc, char* argv []) {
  cerr << "GitRevision: " << GIT_REVISION << endl;
  string usage = "phonetisaurus-g2prnn --rnnlm=test.rnnlm " \
    "--wordlist=test.words --nbest=5\n\n Usage: ";
  set_new_handler (FailedNewHandler);
  PhonetisaurusSetFlags (usage.c_str (), &argc, &argv, false);

  if (FST_FLAGS_rnnlm.compare ("") == 0) {
    cout << "--rnnlm model is required!" << endl;
    exit (1);
  } else {
    std::ifstream rnnlm_ifp (FST_FLAGS_rnnlm);
    if (!rnnlm_ifp.good ()) {
      cout << "Faile to open --rnnlm file '"
	   << FST_FLAGS_rnnlm << "'" << endl;
      exit (1);
    }
  }

  bool use_wordlist = false;
  if (FST_FLAGS_wordlist.compare ("") != 0) {
    std::ifstream wordlist_ifp (FST_FLAGS_wordlist);
    if (!wordlist_ifp.good ()) {
      cout << "Failed to open --wordlist file '" 
	   << FST_FLAGS_wordlist << "'" << endl;
      exit (1);
    } else {
      use_wordlist = true;
    }
  }
      
  if (FST_FLAGS_wordlist.compare ("") == 0 && FST_FLAGS_word.compare ("") == 0) {
    cout << "Either --wordlist or --word must be set!" << endl;
  }
 
#ifdef _OPENMP
  omp_set_num_threads (FST_FLAGS_threads);
#endif
  vector<string> corpus;

  LoadWordList (FST_FLAGS_wordlist, &corpus);

  RMAP rmap;

  LegacyRnnLMReader<Decodable, LegacyRnnLMHash> reader (FST_FLAGS_rnnlm);
  LegacyRnnLMHash h = reader.CopyVocabHash (FST_FLAGS_gdelim, FST_FLAGS_gpdelim);
  Decodable s = reader.CopyLegacyRnnLM (h);

  if (use_wordlist == true) {
    if (FST_FLAGS_threads > 1) {
      ThreadedEvaluateWordlist (corpus, rmap, h, s, FST_FLAGS_threads,
				FST_FLAGS_beam, FST_FLAGS_kmax, FST_FLAGS_nbest,
				FST_FLAGS_reverse, FST_FLAGS_gpdelim,
				FST_FLAGS_gdelim, FST_FLAGS_skip,
				FST_FLAGS_thresh, FST_FLAGS_gsep);
    } else {
      EvaluateWordlist (corpus, h, s, FST_FLAGS_beam, 
			FST_FLAGS_kmax, FST_FLAGS_nbest, FST_FLAGS_reverse, 
			FST_FLAGS_gpdelim, FST_FLAGS_gdelim, FST_FLAGS_skip, 
			FST_FLAGS_thresh, FST_FLAGS_gsep);
    }
  } else {
    EvaluateWord (FST_FLAGS_word, h, s, FST_FLAGS_beam, FST_FLAGS_kmax,
		  FST_FLAGS_nbest, FST_FLAGS_reverse, FST_FLAGS_gpdelim,
		  FST_FLAGS_gdelim, FST_FLAGS_skip, FST_FLAGS_thresh, FST_FLAGS_gsep);
  }

  return 0;
}
