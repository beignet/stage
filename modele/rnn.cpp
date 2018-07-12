#include <sys/types.h>
#include <unistd.h>
#include "rnn.hpp"

using namespace std;
using namespace dynet;

	/* Constructors */

RNN::RNN(unsigned nblayer, unsigned inputdim, unsigned hiddendim, float dropout, unsigned s, ParameterCollection& model) :
	nb_layers(nblayer), input_dim(inputdim), hidden_dim(hiddendim), dropout_rate(dropout), systeme(s)
{
	forward_lstm = new VanillaLSTMBuilder(nb_layers, input_dim, hidden_dim, model); 
	apply_dropout = (dropout != 0);
	p_bias = model.add_parameters({NB_CLASSES});
}

	/* Getters and setters */

void RNN::disable_dropout()
{
	apply_dropout = false;
}

void RNN::enable_dropout()
{
	apply_dropout = true;
}

float RNN::get_dropout_rate(){ return dropout_rate; }
unsigned RNN::get_nb_layers(){ return nb_layers; }
unsigned RNN::get_input_dim(){ return input_dim; }
unsigned RNN::get_hidden_dim(){ return hidden_dim; }

void softmax_vect(vector<float>& tmp, vector<vector<float>>& alpha, unsigned& colonne)
{
	float x,y;
	unsigned j,k;
	vector<float> copy(tmp.size());
	for(j=0; j<tmp.size(); ++j)
	{
		x = exp(tmp[j]);
		y = 0;
		for(k=0; k<tmp.size(); ++k)
			y += exp(tmp[k]);
		copy[j] = x / y; 
	}
	
	for(j=0; j<copy.size() ; ++j)
		alpha[j][colonne] = copy[j];
	++colonne;

}

void softmax_vect(vector<float>& tmp)
{
	float x,y;
	unsigned j,k;
	vector<float> copy(tmp.size());
	for(j=0; j<tmp.size(); ++j)
	{
		x = exp(tmp[j]);
		y = 0;
		for(k=0; k<tmp.size(); ++k)
			y += exp(tmp[k]);
		copy[j] = x / y; 
	}
	
	for(j=0; j<copy.size(); ++j)
		tmp[j] = copy[j];
	
}

	/* Predictions algorithms */

vector<float> RNN::predict_algo(Expression& x, ComputationGraph& cg, bool print_proba, unsigned& argmax)
{
	//vector<float> probs = as_vector(cg.forward(x));
	vector<float> probs = as_vector(cg.forward(x));
	softmax_vect(probs);
	argmax=0;

	for (unsigned k = 0; k < probs.size(); ++k) 
	{
		if(print_proba)
			cerr << "proba[" << k << "] = " << probs[k] << endl;
		if (probs[k] > probs[argmax])
			argmax = k;
	}
	
	return probs;
}

unsigned predict_dev_and_test(RNN& rnn, Data& dev_set, Embeddings& embedding, unsigned nb_of_sentences_dev, unsigned& best)
{
	unsigned positive = 0;
	unsigned positive_inf = 0;
	unsigned positive_neutral = 0;
	unsigned positive_contradiction = 0;
	unsigned label_predicted;
	
	const double nb_of_inf = static_cast<double>(dev_set.get_nb_inf());
	const double nb_of_contradiction = static_cast<double>(dev_set.get_nb_contradiction());
	const double nb_of_neutral = static_cast<double>(dev_set.get_nb_neutral());

	for (unsigned i=0; i<nb_of_sentences_dev; ++i)
	{
		ComputationGraph cg;
		rnn.predict(dev_set, embedding, i, cg, false, label_predicted, NULL);
		if (label_predicted == dev_set.get_label(i))
		{
			positive++;
			if(dev_set.get_label(i) == NEUTRAL)
				++positive_neutral;
			else if(dev_set.get_label(i) == INFERENCE)
				++positive_inf;
			else
				++positive_contradiction;
		}
	}

	cerr << "Accuracy in general = " << positive / (double) nb_of_sentences_dev << endl;
	
	cerr << "\tContradiction Accuracy = " << positive_contradiction / nb_of_contradiction << endl;
	cerr << "\tInference Accuracy = " << positive_inf / nb_of_inf << endl;
	cerr << "\tNeutral Accuracy = " << positive_neutral / nb_of_neutral << endl;	
	
	return positive;
}

		/* Predictions algorithms (handlers) */

void run_predict(RNN& rnn, ParameterCollection& model, Data& test_set, Embeddings& embedding, char* parameters_filename)
{
	// Load preexisting weights
	cerr << "Loading parameters ...\n";
	TextFileLoader loader(parameters_filename);
	loader.populate(model);
	cerr << "Parameters loaded !\n";

	cerr << "Testing ...\n";
	unsigned nb_of_sentences = test_set.get_nb_sentences();
	unsigned best = 0;
	rnn.disable_dropout();
	predict_dev_and_test(rnn, test_set, embedding, nb_of_sentences, best);
}


vector<float> run_predict_for_server_lime(RNN& rnn, Data& test_set, Embeddings& embedding, bool print_label)
{
	//cerr << "Testing ...\n";
	unsigned label_predicted;
	rnn.disable_dropout();
	ComputationGraph cg;
	vector<float> probas = rnn.predict(test_set, embedding, 0, cg, false, label_predicted, NULL);
	cout << "predict ok\n";
	//if(print_label)
		//cerr << "True label = " << test_set.get_label(0) << ", label predicted = " << label_predicted << endl;
	return probas;
}



void write_sentences(ofstream& output, vector<unsigned>& premise, vector<unsigned>& hypothesis)
{
	unsigned i;
	for(i=0; i<premise.size(); ++i)
		output << premise[i] << " ";
	output << "-1\n";
	for(i=0; i<hypothesis.size(); ++i)
		output << hypothesis[i] << " ";	
	output << "-1\n";
}

void save_sentences(Data& explication_set,vector<unsigned>& premise,vector<unsigned>& hypothesis, unsigned num_sample)
{
	for(unsigned sentence=1; sentence <= 2; ++sentence)
	{
		for(unsigned i =0; i<explication_set.get_nb_words(sentence, num_sample); ++i)
		{
			if(sentence==1)
				premise.push_back(explication_set.get_word_id(sentence, num_sample, i));
			else
				hypothesis.push_back(explication_set.get_word_id(sentence, num_sample, i));
		}	
	}
}



/* Sans coeff pour l'instant */
float calculate_DI(vector<float>& probs, vector<float>& original_probs, unsigned label_predicted)
{
	float distance;
	float DI = 0;
	for(unsigned label = 0; label < NB_CLASSES; ++label)
	{
		distance = probs[label] - original_probs[label];
		if(label == label_predicted)
		{
			distance = -distance;
		}
		DI += distance;
	}
	return DI;
}

/* Calcul de DI pour chaque label (comme Lime)*/
void calculate_DI_label(vector<float>& probs, vector<float>& original_probs, vector<float>& DI)
{
	float distance;
	for(unsigned label_ref = 0; label_ref < NB_CLASSES; ++label_ref)
	{
		for(unsigned label = 0; label < NB_CLASSES; ++label)
		{
			distance = probs[label] - original_probs[label];
			if(label == label_ref)
			{
				distance = -distance;
			}
			DI[label_ref] += distance;
		}
	}
}

// surligne les couples importants dans la matrice de co attention
void run_prediction_expl_for_sys_4(RNN& rnn, ParameterCollection& model, Data& explication_set, Embeddings& embedding, char* parameters_filename, char* lexique_filename)
{
	cerr << "Loading parameters ...\n";
	TextFileLoader loader(parameters_filename);
	loader.populate(model);
	cerr << "Parameters loaded !\n";

	cerr << "Testing ...\n";
	unsigned label_predicted;
	unsigned label_predicted_true_sample;
	const unsigned nb_of_sentences = explication_set.get_nb_sentences();
	rnn.disable_dropout();
	char* name = "Files/expl_sys4_token";
	ofstream output(name, ios::out | ios::trunc);
	if(!output)
	{
		cerr << "Problem with the output file " << name << endl;
		exit(EXIT_FAILURE);
	}	
	unsigned important_couple[4] = {0};
	unsigned i,k;
	for(i=0; i < nb_of_sentences; ++i)
	{
		ComputationGraph cg;
		vector<float> original_probs = rnn.predict(explication_set, embedding, i, cg, false, label_predicted_true_sample, important_couple);
		output << explication_set.get_label(i) << endl << label_predicted_true_sample << endl;
		output << original_probs[0] << " " << original_probs[1] << " " << original_probs[2] << endl;
		for(k=0; k<4; k+=2)
			output << important_couple[k] << " " << important_couple[k+1] << "\n";
		explication_set.print_sentences_of_a_sample(i, output);
		output << "-3\n";
	}
	output.close();
	char* name_detok = "Files/expl_sys4_detoken";
	detoken_expl_sys4(lexique_filename, name, name_detok);
}






	/* Negative log softmax algorithms */

Expression get_neg_log_softmax(RNN& rnn, Data& set, Embeddings& embedding, unsigned num_sentence, ComputationGraph& cg)
{
	Expression loss_expr = rnn.get_neg_log_softmax(set, embedding, num_sentence, cg);
	return loss_expr;
}

Expression RNN::get_neg_log_softmax_algo(Expression& score, unsigned num_sentence, Data& set)
{
	const unsigned label = set.get_label(num_sentence);
	Expression loss_expr = pickneglogsoftmax(score, label);

	return loss_expr;
}

	/* Training algorithm */

void run_train(RNN& rnn, ParameterCollection& model, Data& train_set, Data& dev_set, Embeddings& embedding, char* output_emb_filename, unsigned nb_epoch, unsigned batch_size)
{
	Trainer* trainer = new AdamTrainer(model);

	// Model output file
	ostringstream os;
	os << "rnn"
	   << "_D-" << rnn.get_dropout_rate()
	   << "_L-" << rnn.get_nb_layers()
	   << "_I-" << rnn.get_input_dim()
	   << "_H-" << rnn.get_hidden_dim()
	   << "_pid-" << getpid() << ".params";
	string parameter_filename = os.str();
	cerr << "Parameters will be written to: " << parameter_filename << endl;

	unsigned best = 0; 
	unsigned si;
	unsigned nb_samples;
	unsigned nb_of_sentences = train_set.get_nb_sentences();
	unsigned nb_of_sentences_dev = dev_set.get_nb_sentences();

	// Number of batches in training set
	unsigned nb_batches = nb_of_sentences / batch_size; 
	cerr << "nb of examples = "<<nb_of_sentences<<endl;
	cerr << "batches size = "<<batch_size<<endl;
	cerr << "nb of batches = "<<nb_batches<<endl;

	vector<unsigned> order(nb_of_sentences);
	for (unsigned i = 0; i < nb_of_sentences; ++i) 
		order[i] = i;
	unsigned numero_sentence;
	
	for(unsigned completed_epoch=0; completed_epoch < nb_epoch; ++completed_epoch) 
	{
		// Reshuffle the dataset
		cerr << "\n**SHUFFLE\n";
		random_shuffle(order.begin(), order.end());
		Timer iteration("completed in");

		nb_samples=0;
		numero_sentence=0;
		rnn.enable_dropout();
		train_score(rnn, train_set, embedding, nb_batches, nb_samples, batch_size, completed_epoch, nb_of_sentences, trainer, order, numero_sentence);
		rnn.disable_dropout();
		dev_score(rnn, model, dev_set, embedding, parameter_filename, nb_of_sentences_dev, best, output_emb_filename);
	}
}


void train_score(RNN& rnn, Data& train_set, Embeddings& embedding, unsigned nb_batches, 
        unsigned& nb_samples, unsigned batch_size, unsigned completed_epoch, unsigned nb_of_sentences, Trainer* trainer, vector<unsigned>& order, unsigned& numero_sentence)
{
	double loss = 0;

	for(unsigned batch = 0; batch < nb_batches; ++batch)
	{
		ComputationGraph cg; // we create a new computation graph for the epoch, not each item.
		vector<Expression> losses;

		// we will treat all those sentences as a single batch
		for (unsigned si = 0; si < batch_size; ++si) 
		{
			Expression loss_expr = get_neg_log_softmax(rnn, train_set, embedding, order[numero_sentence], cg);
			losses.push_back(loss_expr);
			++numero_sentence;
		}
		// Increment number of samples processed
		nb_samples += batch_size;

		/*  we accumulated the losses from all the batch.
			Now we sum them, and do forward-backward as usual.
			Things will run with efficient batch operations.  */
		Expression sum_losses = (sum(losses)) / (double)batch_size; //averaging the losses
		loss += as_scalar(cg.forward(sum_losses));
		cg.backward(sum_losses); //backpropagation gradient
		trainer->update(); //update parameters

		if(/*(batch + 1) % (nb_batches / 160) == 0 || */batch==nb_batches-1)
		{
			trainer->status();
			cerr << " Epoch " << completed_epoch+1 << " | E = " << (loss / static_cast<double>(nb_of_sentences))
				 << " | samples processed = "<<nb_samples<<endl;
		}
	}
}

void dev_score(RNN& rnn, ParameterCollection& model, Data& dev_set, Embeddings& embedding, string parameter_filename, unsigned nb_of_sentences_dev, unsigned& best, char* output_emb_filename)
{
	unsigned positive = predict_dev_and_test(rnn, dev_set, embedding, nb_of_sentences_dev, best);
	if (positive > best) //save the model if it's better than before
	{
		cerr << "it's better !\n";
		best = positive;
		TextFileSaver saver(parameter_filename);
		saver.save(model);
		if(output_emb_filename != NULL)
			embedding.print_embedding(output_emb_filename);
	}

}

	/* BAXI !!!! */




void explain_label(vector<float>& probs, vector<float>& original_probs, vector<float>& DI, unsigned label_explained, 
	unsigned true_label)
{
	float distance;
	float di_tmp = 0;

	for(unsigned label=0; label < NB_CLASSES; ++label)
	{
		distance = probs[label] - original_probs[label];
		if(label == label_explained)
			distance = -distance;
		di_tmp += distance;
	}
	
	if(di_tmp > DI[label_explained])
		DI[label_explained] = di_tmp;
}



void affichage_vect_DI(vector<float>& vect_DI)
{
	for(unsigned i=0; i<vect_DI.size(); ++i)
		cout << "vect_di[" <<i <<"] =" << vect_DI[i] << endl;
}

void affichage_max_DI(vector<vector<float>>& max_DI)
{
	for(unsigned i=0; i<max_DI.size(); ++i)
	{
		for(unsigned j=0; j<max_DI[i].size(); ++j)
			cout << "max_DI[" <<i << "][" << j << "] = "<< max_DI[i][j] << " ";
		cout << endl;
	}
}

void init_DI_words(unsigned size, vector<vector<float>>& max_DI)
{
	for(unsigned lab=0; lab<NB_CLASSES; ++lab)
		if(size != 0)
			std::fill(max_DI[lab].begin(), max_DI[lab].end(), -99999);
}
unsigned nb_correct(Data& explication_set, vector<unsigned>& save, unsigned num_sample, bool is_premise)
{
	unsigned correct = 0;
	unsigned true_imp_position;
	for(unsigned word = 0; word < save.size(); ++word)
	{
		true_imp_position = explication_set.get_important_words_position(is_premise, num_sample, word);
		if( std::find(save.begin(), save.end(), true_imp_position) != save.end() )
			++correct;
	}
	return correct;
	
}

void mesure(Data& explication_set, vector<unsigned>& correct, unsigned nb_samples)
{
	//float precision;
	float recall;
	//float f_mesure;
	vector<unsigned> nb_label(NB_CLASSES,0);
	unsigned correct_total = 0;
	for(unsigned i=0; i<NB_CLASSES; ++i)
	{
		nb_label[i] = explication_set.get_nb_important_words_in_label(i, nb_samples);
		correct_total += correct[i];	
	}
	
	/*unsigned total_size = explication_set.get_nb_words_total();
	precision = correct_total / (double)total_size;
	*/
	unsigned total_imp_size = explication_set.get_nb_words_imp_total(nb_samples);
	recall = correct_total / (double)total_imp_size;
	//cout << "correct_total = " << correct_total << " total_imp_size = " << total_imp_size << endl;
	//f_mesure = (2 * precision * recall) / (double)(precision + recall);
	
	//cout << "P = "<< precision << "\nR = " << recall << "\nF = " << f_mesure << endl;
	cout << "Accuracy total imp. words = " << recall << endl;
	cout << "\ttotal imp. words = " << total_imp_size << endl;
	cout << "\tAccurracy for neutral = " <<correct[0] <<"/"<< (double)nb_label[0] << endl;
	cout << "\tAccurracy for entailment = " << correct[1] <<"/"<< (double)nb_label[1] << endl;
	cout << "\tAccurracy for contradiction = " << correct[2] <<"/"<< (double)nb_label[2] << endl;
	
}

void imp_words_for_mesure(RNN& rnn, ComputationGraph& cg, Data& explication_set, Embeddings& embedding, unsigned word_position,
	bool is_premise, unsigned word, vector<float>& original_probs, vector<vector<float>>& max_DI, vector<vector<unsigned>>& save, unsigned num_sample,
	vector<Switch_Words*>& sw_vect)
{
	vector<float> vect_DI(NB_CLASSES, -999); // contient DI pour neutral, inf, contradiction
	std::vector<float>::iterator index_min_it;
	unsigned index_min, label_predicted;
	unsigned nb_changing_words;
	unsigned changing_word;
	double proba_log=0;
	vector<float> result(NB_CLASSES, 0.0);
	
	for(unsigned label_explained=0; label_explained < NB_CLASSES; ++label_explained)
	{
		nb_changing_words = sw_vect[label_explained]->get_nb_switch_words(word_position, is_premise, num_sample);
		
		for(unsigned nb=0; nb<nb_changing_words; ++nb)
		{
			cg.clear();
			changing_word = sw_vect[label_explained]->get_switch_word(word_position, is_premise, nb, num_sample);
			explication_set.set_word(is_premise, word_position, changing_word, num_sample);
			vector<float> probs = rnn.predict(explication_set, embedding, num_sample, cg, false, label_predicted, NULL);
			explain_label(probs, original_probs, vect_DI, label_explained, explication_set.get_label(num_sample)); //max de ça = l'importance du mot
		}
	}
		
	for(unsigned lab=0; lab<NB_CLASSES; ++lab)
	{
		index_min_it = std::min_element(max_DI[lab].begin(), max_DI[lab].end());
		index_min = std::distance(std::begin(max_DI[lab]), index_min_it);
		
		
		if(vect_DI[lab] > max_DI[lab][index_min]) 
		{
			max_DI[lab][index_min] = vect_DI[lab];
			save[lab][index_min] = word_position; 
		}										
	}	
	
	explication_set.set_word(is_premise, word_position, word, num_sample);
}

void change_words_for_mesure(RNN& rnn, ParameterCollection& model, Data& explication_set, Embeddings& embedding, char* parameters_filename, char* lexique_filename,
	vector<Switch_Words*>& sw_vect)
{
	cerr << "Loading parameters ...\n";
	TextFileLoader loader(parameters_filename);
	loader.populate(model);
	cerr << "Parameters loaded !\n";

	cerr << "Testing ...\n";
	unsigned label_predicted;
	unsigned label_predicted_true_sample;
	const unsigned nb_of_sentences = explication_set.get_nb_sentences();
	rnn.disable_dropout();
	char* name = "Files/expl_token_changing_word";
	ofstream output(name, ios::out | ios::trunc);
	if(!output)
	{
		cerr << "Problem with the output file " << name << endl;
		exit(EXIT_FAILURE);
	}		
	vector<unsigned> premise;
	vector<unsigned> hypothesis;
	/*vector<vector<unsigned>> save_prem(NB_CLASSES, vector<unsigned>(3));
	vector<vector<unsigned>> save_hyp(NB_CLASSES, vector<unsigned>(3));
	vector<vector<float>> max_DI(NB_CLASSES, vector<float>(3)); // 3 DI for each label*/
	
	
	unsigned prem_size, hyp_size, position;
	float DI;
	unsigned nb_imp_words_prem;
	unsigned nb_imp_words_hyp;
	unsigned true_label;
	vector<unsigned> correct(NB_CLASSES,0);  
	vector<unsigned> nb_label(NB_CLASSES,0);  
	vector<unsigned> positive(NB_CLASSES,0);  
	unsigned pos = 0;  
	
	for(unsigned i=0; i<19; ++i) // POUR L'INSTANT ON EN A FAIT 13
	{
		nb_imp_words_prem = explication_set.get_nb_important_words(true, i);
		nb_imp_words_hyp = explication_set.get_nb_important_words(false, i);
		
		vector<vector<unsigned>> save_prem(NB_CLASSES, vector<unsigned>(nb_imp_words_prem));
		vector<vector<unsigned>> save_hyp(NB_CLASSES, vector<unsigned>(nb_imp_words_hyp));
		vector<vector<float>> max_DI_prem(NB_CLASSES, vector<float>(nb_imp_words_prem));
		vector<vector<float>> max_DI_hyp(NB_CLASSES, vector<float>(nb_imp_words_hyp));
		
		cout << "SAMPLE " << i+1 << "\n";
		ComputationGraph cg;
		save_sentences(explication_set, premise, hypothesis, i);
		
			// original prediction
		true_label = explication_set.get_label(i);
		++nb_label[true_label];
		vector<float> original_probs = rnn.predict(explication_set, embedding, i, cg, false, label_predicted_true_sample, NULL);
		if(label_predicted_true_sample == true_label)
		{
			++pos;
			++positive[label_predicted_true_sample];
		}
			
		output << true_label << endl << label_predicted_true_sample << endl;
		output << original_probs[0] << " " << original_probs[1] << " " << original_probs[2] << endl;
		
			// init DI of words
		init_DI_words(nb_imp_words_prem, max_DI_prem);
		init_DI_words(nb_imp_words_hyp, max_DI_hyp);
		
		prem_size = explication_set.get_nb_words(1,i);
		hyp_size = explication_set.get_nb_words(2,i);
		
		// In the premise
		if(nb_imp_words_prem != 0)
		{
			for( position=0; position<prem_size; ++position)
			{
				imp_words_for_mesure(rnn, cg, explication_set, embedding, position, true, premise[position], original_probs, max_DI_prem, save_prem, i, sw_vect);
				cout << premise[position] << endl;
			}
		}
		cout << "HYP :\n";
		// In the hypothesis
		if(nb_imp_words_hyp != 0)
		{
			for(position=0; position<hyp_size; ++position)
			{	
				imp_words_for_mesure(rnn, cg, explication_set, embedding, position, false, hypothesis[position], original_probs, max_DI_hyp, save_hyp, i, sw_vect);
				cout << hypothesis[position] << endl;
			}
		}
		
		premise.clear();
		hypothesis.clear();
		for(unsigned lab=0; lab<NB_CLASSES; ++lab)
		{
			for(unsigned j=0; j<max_DI_prem[lab].size(); ++j)
				output << save_prem[lab][j] << " ";
			output << "-1\n";
		}
		for(unsigned lab=0; lab<NB_CLASSES; ++lab)
		{
			for(unsigned j=0; j<max_DI_hyp[lab].size(); ++j)
				output << save_hyp[lab][j] << " ";
			output << "-1\n";
		}
		
		explication_set.print_sentences_of_a_sample(i, output);
		output << "-3\n";
		
		/* Calcul pour les taux */
		correct[true_label] +=  nb_correct(explication_set, save_prem[true_label], i, true);  //nb de correct dans la prémisse du sample i
		correct[true_label] +=  nb_correct(explication_set, save_hyp[true_label], i, false);  //nb de correct dans l'hypothèse du sample i
	}	
	//output.close();
	cout << "Success Rate = " << 100 * (pos / (double)19) << endl;
	cout << "\tSuccess Rate neutral = " << 100 * (positive[0] / (double)nb_label[0]) << endl;
	cout << "\tSuccess Rate entailment = " << 100 * (positive[1] / (double)nb_label[1]) << endl;
	cout << "\tSuccess Rate contradiction = " << 100 * (positive[2] / (double)nb_label[2]) << endl;
	
	cout << "\tRate neutral = " << 100 * (nb_label[0] / (double)19) << endl;
	cout << "\tRate entailment = " << 100 * (nb_label[1] / (double)19) << endl;
	cout << "\tRate contradiction = " << 100 * (nb_label[2] / (double)19) << endl;
	
	mesure(explication_set,correct,19);
	output.close();
	char* name_detok = "Files/expl_detoken_changing_word";
	detoken_expl(lexique_filename, name, name_detok);
	//char* name_detok = "Files/expl_detoken_changing_word";
	//detoken_expl(lexique_filename, name, name_detok);
}


	




/*
void write_imp_words(ofstream& output, unsigned position_imp_expr, bool is_premise, Switch_Words* sw_vect, unsigned num_sample)
{
	for(unsigned i=0; i<sw_vect->get_nb_token(is_premise, num_sample, position_imp_expr); ++i)
		output << sw_vect->get_real_word_position(is_premise, num_sample, position_imp_expr, i) << " ";
	
}
void write_in_file(ofstream& output, vector<vector<float>>& max_DI, vector<Switch_Words*>& sw_vect, unsigned num_sample, vector<vector<unsigned>>& save)
{
	for(unsigned lab=0; lab<NB_CLASSES; ++lab)
	{
		for(unsigned j=0; j<max_DI[lab].size(); ++j)
			write_imp_words(output, save[lab][j], false, sw_vect[lab], num_sample);
		output << "-1\n";
	}
}

void write_in_file(ofstream& output, vector<vector<float>>& max_DI, vector<Switch_Words*>& sw_vect, unsigned num_sample, vector<vector<unsigned>>& save, Data& explication_set)
{
	write_in_file(output, max_DI, sw_vect, num_sample, save);
	
	explication_set.print_sentences_of_a_sample(num_sample, output);
	output << "-3\n";
}
*/

