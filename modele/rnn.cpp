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

unsigned predict_dev_and_test(RNN& rnn, DataSet& dev_set, Embeddings& embedding, unsigned nb_of_sentences_dev, unsigned& best)
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

void run_predict(RNN& rnn, ParameterCollection& model, DataSet& test_set, Embeddings& embedding, char* parameters_filename)
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


vector<float> run_predict_for_server_lime(RNN& rnn, DataSet& test_set, Embeddings& embedding, bool print_label, unsigned num_sample)
{
	//cerr << "Testing ...\n";
	unsigned label_predicted;
	rnn.disable_dropout();
	ComputationGraph cg;
	vector<float> probas = rnn.predict(test_set, embedding, num_sample, cg, false, label_predicted, NULL);
	cout << "predict ok\n";
	//if(print_label)
		//cerr << "True label = " << test_set.get_label(0) << ", label predicted = " << label_predicted << endl;
	return probas;
}






	/* Negative log softmax algorithms */

Expression get_neg_log_softmax(RNN& rnn, DataSet& set, Embeddings& embedding, unsigned num_sentence, ComputationGraph& cg)
{
	Expression loss_expr = rnn.get_neg_log_softmax(set, embedding, num_sentence, cg);
	return loss_expr;
}

Expression RNN::get_neg_log_softmax_algo(Expression& score, unsigned num_sentence, DataSet& set)
{
	const unsigned label = set.get_label(num_sentence);
	Expression loss_expr = pickneglogsoftmax(score, label);

	return loss_expr;
}

	/* Training algorithm */

void run_train(RNN& rnn, ParameterCollection& model, DataSet& train_set, DataSet& dev_set, Embeddings& embedding, char* output_emb_filename, unsigned nb_epoch, unsigned batch_size)
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
		// Reshuffle the DataSetset
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


void train_score(RNN& rnn, DataSet& train_set, Embeddings& embedding, unsigned nb_batches, 
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

void dev_score(RNN& rnn, ParameterCollection& model, DataSet& dev_set, Embeddings& embedding, string parameter_filename, unsigned nb_of_sentences_dev, unsigned& best, char* output_emb_filename)
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

void explain_label(vector<float>& probs, vector<float>& original_probs, float& DI, unsigned label_explained)
{
	float distance=0;
	float di_tmp = 0;

	for(unsigned label=0; label < NB_CLASSES; ++label)
	{
		distance = probs[label] - original_probs[label];
		if(label == label_explained)
		{
			distance = -distance;
			di_tmp += distance;
		}
	}
	
	if(di_tmp > DI)
		DI = di_tmp;
}


// Calcule l'importance de chaque expression.
void calcul_importance(RNN& rnn, ComputationGraph& cg, DataSet& explication_set, Embeddings& embedding, unsigned num_expr,
	bool is_premise, vector<float>& original_probs, vector<ExplainationsBAXI>& max_DI, unsigned num_sample, Data* copy)
{
	float DI = -9999; //le DI de l'expression courrante. ça sera le max des DI calculés avec les expr de remplacement.
	
	unsigned index_min, label_predicted;
	unsigned nb_changing_words;
	unsigned changing_word;
	

	nb_changing_words = explication_set.get_nb_switch_words(is_premise, num_expr, num_sample);
	
	//recherche de l'expression de remplacement qui va maximiser le DI ( = le plus important des expr de remplacement)
	for(unsigned nb=0; nb<nb_changing_words; ++nb)
	{
		cg.clear();
		
		explication_set.modif_word(is_premise, num_expr, nb, num_sample);
		
		vector<float> probs = rnn.predict(explication_set, embedding, num_sample, cg, false, label_predicted, NULL);
		explain_label(probs, original_probs, DI, explication_set.get_label(num_sample)); //max de ça = l'importance du mot. 
		
		explication_set.reset_data(*copy, num_sample);
	}
	
		
	max_DI.push_back(ExplainationsBAXI(num_expr, is_premise, DI)); 
	
}



void write_explainations(ofstream& output, vector<ExplainationsBAXI>& max_DI, Detokenisation& detok, Data* copy)
{
	for(unsigned i=0; i < max_DI.size(); ++i)
		output << "('" << detok.detoken(max_DI[i].num_expr, max_DI[i].is_premise, *copy) << "'), " <<  max_DI[i].DI << endl;
	output << "-3\n\n\n";
}


void change_words_for_mesure(RNN& rnn, ParameterCollection& model, DataSet& explication_set, Embeddings& embedding, char* parameters_filename, char* lexique_filename)
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
	
	
	unsigned prem_size, hyp_size, position;
	float DI;
	unsigned nb_imp_words_prem;
	unsigned nb_imp_words_hyp;
	unsigned true_label;
	
	
	vector<unsigned> positive(NB_CLASSES,0); 
	unsigned pos = 0;  
	Data* copy=NULL;
	Detokenisation detok(lexique_filename);
	
	for(unsigned i=0; i<nb_of_sentences; ++i) // pour chaque instance...
	{
		vector<ExplainationsBAXI> max_DI;
		
		cout << "SAMPLE " << i << "\n";
		ComputationGraph cg;
		copy = new Data( *(explication_set.get_data_object(i)) ); // COPY DATA
		
			// original prediction
		true_label = explication_set.get_label(i);
		vector<float> original_probs = rnn.predict(explication_set, embedding, i, cg, false, label_predicted_true_sample, NULL);
		if(label_predicted_true_sample == true_label)
		{
			++pos;
			++positive[label_predicted_true_sample];
		}
			
		output << true_label << endl << label_predicted_true_sample << endl;
		output << "neutral : " << original_probs[0] << ", entailment : " << original_probs[1] << ", contradiction : " << original_probs[2] << endl;
		
		// In the premise
		for( position=0; position < explication_set.get_nb_expr(true,i); ++position)
		{
			if(explication_set.expr_is_important(i, true, position))
				calcul_importance(rnn, cg, explication_set, embedding, position, true, original_probs, max_DI, i, copy);
			//cout << premise[position] << endl;
		}
		
		
		cout << "HYP :\n";
		// In the hypothesis
		for(position=0; position < explication_set.get_nb_expr(false,i); ++position)
		{	
			if(explication_set.expr_is_important(i, false, position))
				calcul_importance(rnn, cg, explication_set, embedding, position, false, original_probs, max_DI, i, copy);
			//cout << hypothesis[position] << endl;
		}
		
		sort(max_DI.begin(), max_DI.end(), greater<ExplainationsBAXI>()); 
		write_explainations(output, max_DI, detok, copy);
		
		
	}	
	output.close();
	cout << "Success Rate = " << 100 * (pos / (double)nb_of_sentences) << endl;
	cout << "\tSuccess Rate neutral = " << 100 * (positive[0] / (double)explication_set.get_nb_neutral()) << endl << "\t" << positive[0] << " / "<< explication_set.get_nb_neutral() << endl;
	cout << "\tSuccess Rate entailment = " << 100 * (positive[1] / (double)explication_set.get_nb_inf()) << endl << "\t" << positive[1] << " / "<< explication_set.get_nb_inf() << endl;
	cout << "\tSuccess Rate contradiction = " << 100 * (positive[2] / (double)explication_set.get_nb_contradiction()) << endl << "\t" << positive[2] << " / "<< explication_set.get_nb_contradiction() << endl;

}


