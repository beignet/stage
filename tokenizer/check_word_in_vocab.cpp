#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cstdlib>
#include <map>
#include <algorithm>
#include <locale>

using namespace std;

//sert pour tokenizer les mots de remplacements

void reading_lexique(char* lexique_filename, map<string, int>& word_to_id)
{
	ifstream lexique_file(lexique_filename, ios::in);
	if(!lexique_file)
	{ 
		cerr << "Impossible to open the file " << lexique_filename << endl;
		exit(EXIT_FAILURE);
	}
	string word;
	int id;
	while(lexique_file >> word && lexique_file >> id)
		word_to_id[word] = id;
	word_to_id["-1"] = -1;
	word_to_id["-2"] = -2;
	word_to_id["-3"] = -3;
	cerr << lexique_filename << " has been read" << endl;
	lexique_file.close();	
}

/* *//*
void check(char* lexique, char* file, char* output_f, bool specific)
{
	ifstream f(file, ios::in);
	if(!f)
	{ 
		cerr << "Impossible to open the file " << file << endl;
		exit(EXIT_FAILURE);
	}		
	ofstream output(output_f, ios::out | ios::trunc);
	if(!output)
	{
		cerr << "Problem with the output file "<< output_f << endl;
		exit(EXIT_FAILURE);
	}
	map<string, int> word_to_id;
	reading_lexique(lexique, word_to_id);
	if(specific)
	{
		word_to_id["NEU"] = -2;
		word_to_id["SYN"] = -3;
	}
	map< string, int >::iterator it;
	string word;
	while(f >> word)
	{
		it = word_to_id.find(word);
		if( it == word_to_id.end() )
			output << "INCONNU ";
		else
			output << word_to_id[word] << " ";
	}
	
	f.close();
}*/

void tokeniser_nouveau_remplacement(map<string, int>& word_to_id, char* file, char* output_f)
{
	ifstream f(file, ios::in);
	if(!f)
	{ 
		cerr << "Impossible to open the file " << file << endl;
		exit(EXIT_FAILURE);
	}		
	ofstream output(output_f, ios::out | ios::trunc);
	if(!output)
	{
		cerr << "Problem with the output file "<< output_f << endl;
		exit(EXIT_FAILURE);
	}

	map< string, int >::iterator it;
	string word;
	while(f >> word)
	{
		it = word_to_id.find(word);
		if( it == word_to_id.end() )
			output << "INCONNU ";
		else
			output << word_to_id[word] << " ";
		if(word == "-1" || word == "-2" || word == "-3")
			output << endl;
	}
	
	f.close();	
}

int main(int argc, char** argv)
{
	if(argc != 8)
	{
		cerr << "Usage :\n " << argv[0] << "\n\n"
			 << "lexique\n"
			 << "file mots remplacant detoken neutral\n"
			 << "output neutral(file mots remplacant token)\n\n"
			 << "file mots remplacant detoken inf\n"
			 << "output inf(file mots remplacant token)\n\n"
			 << "file mots remplacant detoken contrad\n"
			 << "output contrad(file mots remplacant token)\n";
		exit(EXIT_FAILURE);
	}
	
	map<string, int> word_to_id;
	reading_lexique(argv[1], word_to_id);
	
	tokeniser_nouveau_remplacement(word_to_id, argv[2], argv[3]);
	tokeniser_nouveau_remplacement(word_to_id, argv[4], argv[5]);
	tokeniser_nouveau_remplacement(word_to_id, argv[6], argv[7]);
	return 0;
}
