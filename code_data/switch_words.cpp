#include "switch_words.hpp"
#include <algorithm>

using namespace std;


Switch_Words::Switch_Words(char* filename)
{
	ifstream database(filename, ios::in);
	if(!database)
	{ 
		cerr << "Impossible to open the file " << filename << endl;
		exit(EXIT_FAILURE);
	}
	int word;
	while(database >> word)
	{
		SW premise(database);
		prem.push_back(premise);
		SW hypothesis(database);
		hyp.push_back(hypothesis);
	}
	
	database.close();
}

RW::RW(unsigned w, unsigned p, int in)
{
	word = w;
	position = p;
	if(in == -4)
		insert = true;
	else
		insert = false;
}

bool RW::is_insert() { return insert; }
unsigned RW::get_word() { return word; }
unsigned RW::get_position() { return position; }


SW::SW(ifstream& database)
{
	int word, nb_words, i, insertion;
	unsigned position;
	bool is_premise = true, ok = true;
	while(ok && database >> nb_words) //read a 'real' word (appearing in the sentences test file)
	{
		if(nb_words == -2)
		{
			ok = false;
			continue;
		}
		
		// Gère les vrais mots (ceux dans la phrase de base)
		for(i=0; i<nb_words; ++i)
		{
			database >> word;
			database >> position;
			real_words.push_back(make_pair(static_cast<unsigned>(word), position));
		}
		
		// Gère les mots/expressions de remplacement
		database >> nb_words; //nb de mots/d'expressions pouvant le remplacer	
		for(i=0; i<nb_words; ++i)
		{
			vector<RW> tmp;
			database >> word;
			while(word!=-1)
			{
				database >> position;
				database >> insertion;
				tmp.push_back(RW(word, position, insertion));
				database >> word;
			}
			remplacing_words.push_back(tmp);
		}		
		
	}
}

unsigned SW::get_word(unsigned num_remplace, unsigned num_w)
{
	return remplacing_words[num_remplace][num_w].get_word();
}
unsigned SW::get_nb_replace() //nb de lignes (nb d'expressions pouvant remplacer)
{
	return remplacing_words.size();
}
unsigned SW::get_nb_replace_word(unsigned num_remplace) //nb de mots pour la ligne "num_remplace" (nb de mots dans l'expression courrante)
{
	return remplacing_words[num_remplace].size();
}

unsigned SW::get_position(unsigned num_remplace, unsigned num_w)
{
	return remplacing_words[num_remplace][num_w].get_position();
}
unsigned SW::is_insert(unsigned num_remplace, unsigned num_w)
{
	return remplacing_words[num_remplace][num_w].is_insert();
}


unsigned Switch_Words::get_word(unsigned num_remplace, unsigned num_w, bool is_premise, unsigned num_sample)
{
	if(is_premise)
		return prem[num_sample].get_word(num_remplace,num_w);
	return hyp[num_sample].get_word(num_remplace,num_w);
}
unsigned Switch_Words::get_nb_replace_word(unsigned num_remplace, bool is_premise, unsigned num_sample)
{
	if(is_premise)
		return prem[num_sample].get_nb_replace_word(num_remplace);
	return hyp[num_sample].get_nb_replace_word(num_remplace);
}
unsigned Switch_Words::get_nb_replace(bool is_premise, unsigned num_sample)
{
	if(is_premise)
		return prem[num_sample].get_nb_replace();
	return hyp[num_sample].get_nb_replace();
}

unsigned Switch_Words::get_position(unsigned num_remplace, unsigned num_w, bool is_premise, unsigned num_sample)
{
	if(is_premise)
		return prem[num_sample].get_position(num_remplace,num_w);
	return hyp[num_sample].get_position(num_remplace,num_w);
}

unsigned Switch_Words::is_insert(unsigned num_remplace, unsigned num_w, bool is_premise, unsigned num_sample)
{
	if(is_premise)
		return prem[num_sample].is_insert(num_remplace,num_w);
	return hyp[num_sample].is_insert(num_remplace,num_w);
}


