#include "data.hpp"
#include <algorithm>

using namespace std;
using namespace dynet;

/**
 * \file data.cpp
*/

/* DATA CLASS */

/**
	* \brief Data constructor
	* 
	* \param data_filename : File containing the samples in this form :
	*       label
	*       premise
	*       hypothesis
*/
Data::Data(char* data_filename)
{
	ifstream data_file(data_filename, ios::in);
	if(!data_file)
	{ 
		cerr << "Impossible to open the file " << data_filename << endl;
		exit(EXIT_FAILURE);
	}
	int val;
	cerr << "Reading data from " << data_filename << " ...\n";

	while(data_file >> val) //read a label
	{
		label.push_back(val);
		init_rate(val);
		
		for(unsigned sentence=0; sentence<2; ++sentence)
		{
			vector<unsigned> tmp_data;
			data_file >> val; //read a word id
			while(val != -1)
			{
				tmp_data.push_back(static_cast<unsigned>(val));
				data_file >> val;
			}
			data_file >> val; //read the sentence's length
			if(sentence==0)
				premise.push_back(tmp_data);
			else
				hypothesis.push_back(tmp_data);
		}
	}
	
	data_file.close();
}

Data::Data(char* test_explication_filename, unsigned mode)
{
	ifstream test_explication(test_explication_filename, ios::in);
	if(!test_explication)
	{ 
		cerr << "Impossible to open the file " << test_explication_filename << endl;
		exit(EXIT_FAILURE);
	}
	int val;
	cerr << "Reading data from " << test_explication_filename << " ...\n";
	//unsigned cpt=0;
	while(test_explication >> val) //read a label
	{
		label.push_back(val);
		//init_rate(val);
		
		for(unsigned sentence=0; sentence<2; ++sentence)
		{
			vector<unsigned> tmp_data;
			test_explication >> val; //read a word id
			while(val != -1)
			{
				tmp_data.push_back(static_cast<unsigned>(val));
				test_explication >> val;
			}
			test_explication >> val; //read the sentence's length
			if(sentence==0)
				premise.push_back(tmp_data);
			else
				hypothesis.push_back(tmp_data);
		}
		Couple cpl(test_explication);
				
		/*cerr << "couples du sample " << cpt+1 << endl;
		++cpt;
		cpl.print_couples();*/
		important_couples.push_back(cpl);
	}
	
	test_explication.close();
}

/* Si all == true ==> on enlève tout */
void Data::remove_couple(vector<unsigned>& num_couple, unsigned num_sample)
{
	unsigned word;
	int position;
	for(unsigned nb_couples=0; nb_couples < num_couple.size(); ++nb_couples) //parcourt des couples à supprimer (ex 0, 1, 2) dans le vecteur passé en arguments
	{
		/* Parcours les couples à supprimer, spécifier dans num_couple
		 * ex je veux enlever les couples 0, 1, 2 :
		 * on parcourt imp_word_premise[nb_couple][word] !
		 */
		//cerr << "couple numero " << num_couple[nb_couples] << endl;
		if( num_couple[nb_couples] >= important_couples[num_sample].get_size() ) //ne pas supprimer un couple qui n'existe pas
			continue;
		for(word=0; word < important_couples[num_sample].get_nb_words(num_couple[nb_couples], true); ++word) // parcours des mots du couple (partie prémisse)
		{
			position = important_couples[num_sample].get_position(num_couple[nb_couples], word, true);
			if(position != -4)
				premise[num_sample][position] = 0;
		}
		//idem pour l'hypothèse :
		for(word=0; word < important_couples[num_sample].get_nb_words(num_couple[nb_couples], false); ++word) // parcours des mots du couple (partie hypothèse)
		{
			position = important_couples[num_sample].get_position(num_couple[nb_couples], word, false);
			if(position != -4)
				hypothesis[num_sample][position] = 0;
		}
				
	}
	
}

void Data::reset_couple(vector<unsigned>& num_couple, unsigned num_sample)
{
	unsigned word;
	int position;
	for(unsigned nb_couples=0; nb_couples < num_couple.size(); ++nb_couples) //parcourt des couples à remettre (ex 0, 1, 2) dans le vecteur passé en arguments
	{
		//cerr << "couple numero " << num_couple[nb_couples] << endl;
		if( num_couple[nb_couples] >= important_couples[num_sample].get_size() ) //ne pas remettre un couple qui n'existe pas
			continue;
		for(word=0; word < important_couples[num_sample].get_nb_words(num_couple[nb_couples], true); ++word) // parcours des mots du couple (partie prémisse)
		{
			position = important_couples[num_sample].get_position(num_couple[nb_couples], word, true);
			if(position != -4)
				premise[num_sample][position] = important_couples[num_sample].get_id(num_couple[nb_couples], word, true);
		}
		//idem pour l'hypothèse :
		for(word=0; word < important_couples[num_sample].get_nb_words(num_couple[nb_couples], false); ++word) // parcours des mots du couple (partie hypothèse)
		{
			position = important_couples[num_sample].get_position(num_couple[nb_couples], word, false);
			if(position != -4)
				hypothesis[num_sample][position] = important_couples[num_sample].get_id(num_couple[nb_couples], word, false);
		}
				
	}	
}

void Data::taking_couple(unsigned num_couple, unsigned num_sample)
{
	if( num_couple >= important_couples[num_sample].get_size() )
	{
		cerr << "Error : couple num " << num_couple << " doesn't exist (nb of couples for the sample " 
			 << num_sample << " = " << important_couples[num_sample].get_size() << ")\n";
		return;
	}
	unsigned word;
	unsigned word_in_sentences;
	bool remove;
	for(word_in_sentences=0; word_in_sentences < premise[num_sample].size(); ++word_in_sentences)
	{
		remove = true;
		for(word=0; word < important_couples[num_sample].get_nb_words(num_couple, true) && remove; ++word) // parcours des mots du couple (partie prémisse)
		{
			if(static_cast<int>(word_in_sentences) != important_couples[num_sample].get_position(num_couple, word, true))
				remove = true;
			else
				remove = false;
		}
		if(remove)	
			premise[num_sample][word_in_sentences] = 0;
	}
	
	//idem pour l'hypothèse :
	for(word_in_sentences=0; word_in_sentences < hypothesis[num_sample].size(); ++word_in_sentences)
	{
		remove = true;
		for(word=0; word < important_couples[num_sample].get_nb_words(num_couple, false) && remove; ++word) // parcours des mots du couple (partie prémisse)
		{
			if(static_cast<int>(word_in_sentences) != important_couples[num_sample].get_position(num_couple, word, false))
				remove = true;
			else
				remove = false;
		}
		if(remove)	
			hypothesis[num_sample][word_in_sentences] = 0;
	}
}

void Data::reset_sentences(vector<unsigned>& original_premise,vector<unsigned>& original_hypothesis, unsigned num_sample, bool is_premise)
{
	if(is_premise)
		for(unsigned j=0; j<original_premise.size(); ++j)
			premise[num_sample][j] = original_premise[j];
	else
		for(unsigned j=0; j<original_hypothesis.size(); ++j)
			hypothesis[num_sample][j] = original_hypothesis[j];

}

bool Data::is_empty(unsigned num_sample, bool is_premise)
{
	if(is_premise)
	{
		for(unsigned i=0; i<premise[num_sample].size(); ++i)
		{
			
			if(premise[num_sample][i] != 0)
				return false;
		}
		cerr << endl;
	}
	else
	{
		for(unsigned i=0; i<hypothesis[num_sample].size(); ++i)
		{
			
			if(hypothesis[num_sample][i] != 0)
				return false;
		}
		cerr << endl;
	}
	return true;
}

/* Ex couple :
	22 15 8 16 507 17 -2 507 1 -1
	0 -4 -2 8634 3 -1
	0 -4 -2 5524 6 -1
	-3
*/
Couple::Couple(ifstream& test_explication)
{
	int val = 0;
	bool ok;
	int position = 0;
	while(val != -3) //for a sample...
	{
		test_explication >> val;
		if(val == -3)
			continue;
		test_explication >> position;
		ok = true;
		vector<pair<unsigned,int>> tmp;
		while(ok && val != -1)
		{
			tmp.push_back(make_pair(static_cast<unsigned>(val) , position));
			test_explication >> val;
			if(val == -2)
			{
				imp_word_premise.push_back(tmp);
				ok = false;
			}
			if(ok && val != -1)
				test_explication >> position;
		}
		if(val == -1)
			imp_word_hypothesis.push_back(tmp);
	}	
}

void Couple::print_couples()
{
	cerr << "Premise : \n";
	/*for(unsigned i=0; i<imp_word_premise.size(); ++i)
	{*/
	unsigned i=1;
		for(unsigned j=0; j<imp_word_premise[i].size(); ++j)
			cerr << imp_word_premise[i][j].first << " POSITION[" << imp_word_premise[i][j].second << "] ";
		cerr << endl;
	//}
	cerr << "Hypothesis : \n";
	/*for(unsigned i=0; i<imp_word_hypothesis.size(); ++i)
	{*/
		for(unsigned j=0; j<imp_word_hypothesis[i].size(); ++j)
			cerr << imp_word_hypothesis[i][j].first << " POSITION[" << imp_word_hypothesis[i][j].second << "] ";
		cerr << endl;
	//}
	cerr<<endl;
}


unsigned Couple::get_id(unsigned num_couple, unsigned num_mot, bool premise)
{
	if(premise)
		return imp_word_premise[num_couple][num_mot].first; 
	return imp_word_hypothesis[num_couple][num_mot].first;
}

int Couple::get_position(unsigned num_couple, unsigned num_mot, bool premise)
{
	if(premise)
		return imp_word_premise[num_couple][num_mot].second; 
	return imp_word_hypothesis[num_couple][num_mot].second;
}

unsigned Couple::get_nb_words(unsigned num_couple, bool premise)
{
	if(premise)
		return imp_word_premise[num_couple].size(); 
	return imp_word_hypothesis[num_couple].size();
}

unsigned Couple::get_size()
{
	return imp_word_hypothesis.size();
}

unsigned Data::get_nb_couple(unsigned num_sample)
{
	return important_couples[num_sample].get_size();
}

unsigned Data::get_couple_nb_words(unsigned num_sample, unsigned num_couple, bool premise)
{
	return important_couples[num_sample].get_nb_words(num_couple, premise);
}

unsigned Data::get_couple_id(unsigned num_sample, unsigned num_couple, unsigned num_mot, bool premise)
{
	return important_couples[num_sample].get_id(num_couple, num_mot, premise);
}

Data::Data(unsigned mode)
{
	//read the label
	int labels;
	cin >> labels; 
	if(labels > 2 || labels < 0)
	{
		cerr << "label must be : (0 : neutral, 1 : inference, or 2 : contradiction)\n";
		exit(EXIT_FAILURE);
	}
	label.push_back(static_cast<unsigned>(labels));
	
	int val=0;
	while(val != -2)
	{
		for(unsigned sentence=0; sentence<2; ++sentence)
		{
			vector<unsigned> tmp_data;
			cin >> val; //read a word id
			while(val != -1)
			{
				/*if(val == -2 && sentence==0)
					exit(EXIT_FAILURE);*/
				tmp_data.push_back(static_cast<unsigned>(val));
				cin >> val;
			}
			if(sentence==0)
				premise.push_back(tmp_data);
			else
				hypothesis.push_back(tmp_data);
		}
		cin >> val; //read -2
	}
	
}

Data::Data(unsigned mode, char* lexique_filename)
{
	//read the label
	int labels;
	cin >> labels; 
	if(labels > 2 || labels < 0)
	{
		cerr << "label must be : (0 : neutral, 1 : inference, or 2 : contradiction)\n";
		exit(EXIT_FAILURE);
	}
	label.push_back(static_cast<unsigned>(labels));
	
	ifstream lexique_file(lexique_filename, ios::in);
	if(!lexique_file)
	{ 
		cerr << "Impossible to open the file " << lexique_filename << endl;
		exit(EXIT_FAILURE);
	}
	
	cerr << "Reading " << lexique_filename << endl;
	
	string word;
	unsigned id;
	map<string, unsigned> word_to_id;
	while(lexique_file >> word && lexique_file >> id)
		word_to_id[word] = id;
	cerr << lexique_filename << " has been read" << endl;

	for(unsigned sentence=0; sentence<2; ++sentence)
	{
		vector<unsigned> tmp_data;
		cin >> word; //read a word
		while(word != "-1")
		{
			std::transform(word.begin(), word.end(), word.begin(), ::tolower); 
			//cerr << word << " ";
			tmp_data.push_back( word_to_id[word] );
			cin >> word;
		}
		if(sentence==0)
			premise.push_back(tmp_data);
		else
			hypothesis.push_back(tmp_data);
	}
	
}

void inline Data::init_rate(unsigned label)
{
	switch(label)
	{
		case NEUTRAL:
		{
			++nb_neutral;
			break;
		}
		case INFERENCE:
		{
			++nb_inference;
			break;
		}
		case CONTRADICTION:
		{
			++nb_contradiction;
			break;
		}
	}
}

unsigned Data::get_nb_contradiction() { return nb_contradiction; }
unsigned Data::get_nb_inf() { return nb_inference; }
unsigned Data::get_nb_neutral() { return nb_neutral; }

void Data::print_infos(unsigned type)
{
	const float nb_samples = static_cast<float>(label.size());
	
	if(type==0)
		cerr << "**TRAIN SET**\n";
	else if(type==1)
		cerr << "**DEV SET**\n";
	else
		cerr << "**TEST SET**\n";
	cerr << "\tNumber of samples = " << label.size() << endl;	
	cerr << "\tContradiction rate = " << nb_contradiction / nb_samples << endl;
	cerr << "\tInference rate = " << nb_inference / nb_samples << endl;
	cerr << "\tNeutral rate = " << nb_neutral / nb_samples << endl;
}

/**
	* \name get_word_id
	* \brief Give the word id
	* 
	* \param sentence : The sentence from where you pick the word (1 if you want the premise, 2 if you want the hypothesis)
	* \param num_sentence : The number of the sample processed
	* \param word_position : The position of the word
	* 
	* \return The word id
*/
unsigned Data::get_word_id(unsigned sentence, unsigned num_sentence, unsigned word_position)
{
	if(sentence==1)
		return premise[num_sentence][word_position];
	return hypothesis[num_sentence][word_position];
}

/**
	* \name get_label
	* \brief Give the label
	* 
	* \param num_sentence : The number of the sample processed
	* 
	* \return The label
*/
unsigned Data::get_label(unsigned num_sentence)
{
	return label[num_sentence];
}

/**
	* \name get_nb_words
	* \brief Give the number of words in the sentence
	* 
	* \param sentence : The sentence from where you want to know the number of words 
	* (1 if you want the premise, 2 if you want the hypothesis)
	* \param num_sentence : The number of the sample processed
	* 
	* \return The number of words in the sentence
*/
unsigned Data::get_nb_words(unsigned sentence, unsigned num_sentence)
{
	if(sentence==1)
		return premise[num_sentence].size();
	return hypothesis[num_sentence].size();
}

/**
	* \name get_nb_sentences
	* \brief Give the number of samples
	* 
	* \return The number of samples
*/
unsigned Data::get_nb_sentences()
{
	return premise.size();
}

/**
	* \name print_sentences
	* \brief Print the entire dataset in a file in this form :
	*       label
	*       premise
	*       hypothesis
	* 
	* Just to debug.
	* \param name : The name of the output file
*/
void Data::print_sentences(char const* name)
{
	ofstream output_file(name, ios::out | ios::trunc);
	if(!output_file)
	{
		cerr << "Problem with the output file "<< name << endl;
		exit(EXIT_FAILURE);
	}
	unsigned k;
	unsigned j;
	for(unsigned i=0; i<premise.size(); ++i)
	{
		output_file << label[i] << endl;
		for(k=0; k<2; ++k)
		{
			if(k==0)
			{
				for(j=0; j<premise[i].size(); ++j)
					output_file << premise[i][j] <<' ';
				output_file << endl;
			}
			else
			{
				for(j=0; j<hypothesis[i].size(); ++j)
					output_file << hypothesis[i][j] <<' ';
				output_file << endl;
			}
		}
	}
	output_file.close();
}

void Data::print_sentences_of_a_sample(unsigned num_sample)
{

	unsigned j;
	cerr << "premise = ";

	for(j=0; j<premise[num_sample].size(); ++j)
		cerr << premise[num_sample][j] <<' ';
	cerr << endl;
	
	cerr << "hypothesis = ";

	for(j=0; j<hypothesis[num_sample].size(); ++j)
		cerr << hypothesis[num_sample][j] <<' ';
	cerr << endl;
	
}

/* EMBEDDING CLASS */

/**
	* \brief Embeddings Constructor (training phase -- random embedding)
	*
	* \param model : The model
	* \param dim : The dimension of the embeddings
*/
Embeddings::Embeddings(ParameterCollection& model, unsigned dim) : p_c(), dim_embedding(dim)
{
	cerr<< "Initializing embeddings...\n";
	p_c = model.add_lookup_parameters(VOCAB_SIZE, {dim_embedding});
	vector<float> vector_zero(dim_embedding, 0); 
	p_c.initialize(0, vector_zero);
}

/**
	* \brief Embeddings Constructor (testing phase)
	* Initialize the word embedding with a file
	* 
	* \param embedding_filename : File containing the embedding
	* \param model : The model
	* \param dim : The dimension of the embeddings
*/
Embeddings::Embeddings(char* embedding_filename, ParameterCollection& model, unsigned dim, bool testing) : p_c(), dim_embedding(dim)
{
	ifstream emb_file(embedding_filename, ios::in);
	if(!emb_file)
	{ 
		cerr << "Impossible to open the file " << embedding_filename << endl;
		exit(EXIT_FAILURE);
	}
	p_c = model.add_lookup_parameters(VOCAB_SIZE, {dim_embedding});
	unsigned tmp;
	string word;
	unsigned index;
	vector<float> embedding(dim_embedding);
	if(testing) index=0;
	else index=1;
	while( emb_file >> word )
	{
		for(unsigned i=0; i<dim_embedding; ++i)
			emb_file >> embedding[i];
		p_c.initialize(index, embedding);
		++index;
	}
	emb_file.close();
}

/**
	* \name print_embedding
	* \brief Print the embedding of each words in a file. Just to debug.
	*
	* \param name : The name of the output file
*/
void Embeddings::print_embedding(char* output_filename)
{
	ComputationGraph cg;
	ofstream output_file(output_filename, ios::out | ios::trunc);
	if(!output_file)
	{
		cerr << "Problem with the output file " << output_filename << endl;
		exit(EXIT_FAILURE);
	}
	cerr << "Printing embeddings on " << output_filename << "...\n";
	for(unsigned i=0; i<VOCAB_SIZE; ++i)
		output_file << "c " << const_lookup(cg, p_c, i).value() << endl; //adding 'c' because the constructor read an emb file with each lines begining with a word

	output_file.close();

}

/**
	* \name get_embedding_expr
	* \brief Give the embedding of the word
	*
	* \param cg : The computation graph
	* \param index : The id of the word
	* 
	* \return The embedding of the word as an Expression
*/
Expression Embeddings::get_embedding_expr(ComputationGraph& cg, unsigned index)
{
	int num = static_cast<int>(index);
	if(num > VOCAB_SIZE)
	{
		cerr << "the vocab size need to be bigger (vocab size = " << VOCAB_SIZE <<", word id = "<< num << endl;
		exit(EXIT_FAILURE);
	}
	/*
	if(index > 28649)
		return const_lookup(cg, p_c, num);*/
	return lookup(cg, p_c, num);
}
