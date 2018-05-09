#include "BILSTM.hpp"
	
using namespace std;
using namespace dynet;
	
	/* Constructors */
	
BiLSTM::BiLSTM(unsigned nblayer, unsigned inputdim, unsigned hiddendim, float dropout, unsigned s, ParameterCollection& model) :
	RNN(nblayer, inputdim, hiddendim, dropout, s, model)
{
	backward_lstm = new VanillaLSTMBuilder(nb_layers, input_dim, hidden_dim, model);
	if(systeme==3)
		p_W = model.add_parameters({NB_CLASSES, 4*hidden_dim});
	else if(systeme==4)
	{
		p_W = model.add_parameters({NB_CLASSES, 2*hidden_dim});
		p_W_attention = model.add_parameters({2*hidden_dim, 2*hidden_dim});
	}
}
	
	/* Predictions algorithms */
	
vector<float> BiLSTM::predict(Data& set, Embeddings& embedding, unsigned num_sentence, ComputationGraph& cg, bool print_proba, unsigned& argmax)
{
	//cerr << "BiLSTM prediction \n";
	Expression x;
	if(systeme==3)
		x = run_KIM(set, embedding, num_sentence, cg);
	else if(systeme==4)
		x = run_sys4(set, embedding, num_sentence, cg);
		
	vector<float> probs = predict_algo(x, cg, print_proba, argmax);
	return probs;
}

	/* Negative log softmax algorithms */

Expression BiLSTM::get_neg_log_softmax(Data& set, Embeddings& embedding, unsigned num_sentence, ComputationGraph& cg)
{
	Expression score;
	if(systeme==3)
		score = run_KIM(set, embedding, num_sentence, cg);
	else if(systeme==4)
		score = run_sys4(set, embedding, num_sentence, cg);
	Expression loss_expr = get_neg_log_softmax_algo(score, num_sentence, set);
	return loss_expr;
}

	/* Bi-LSTM system */

/* Call this fonction twice (the first time for the premise, the second time for the hypothesis) 
 * sentence_repr[i] = representation of the word nb i by the KIM method
 */
/**
        * \brief Give a representation for each word of the given sentence
        * \details Use a Bi-LSTM to run a forward and backward LSTM on the sentence. 
        * The hidden states generated by the LSTMs at each time step are concatenated.
        * 
        * \param embedding : the words embedding
        * \param set : the dataset 
        * \param sentence : The sentence you want to represent (1 if you want the premise, 2 if you want the hypothesis)
        * \param cg : the computation graph
        * \param num_sentence : the number of the sample processed
        * \param sentence_repr : matrix of size (number of words in the sentence, hidden dimension). 
        * sentence_repr[i] = representation of the i_th word
*/
void BiLSTM::words_representation(Embeddings& embedding, Data& set, unsigned sentence,
	ComputationGraph& cg, unsigned num_sentence, vector<Expression>& sentence_repr)
{
	const unsigned nb_words = set.get_nb_words(sentence, num_sentence);
	if (apply_dropout)
	{ 
		forward_lstm->set_dropout(dropout_rate);
		backward_lstm->set_dropout(dropout_rate);
	}
	else 
	{
		forward_lstm->disable_dropout();
		backward_lstm->disable_dropout();
	}
	forward_lstm->new_graph(cg);  // reset LSTM builder for new graph
	forward_lstm->start_new_sequence(); //to do before add_input() and after new_graph()
	backward_lstm->new_graph(cg);  // reset LSTM builder for new graph
	backward_lstm->start_new_sequence(); //to do before add_input() and after new_graph()

	vector<Expression> tmp;
	unsigned i;
	int j;

	/* Run forward LSTM */
	for(i=0; i<nb_words; ++i)
	{
		if(set.get_word_id(sentence, num_sentence, i) == 0)
			continue;
		sentence_repr.push_back(forward_lstm->add_input( embedding.get_embedding_expr(cg, set.get_word_id(sentence, num_sentence, i)) ) );
	}
	/* Run backward LSTM */
	for(j=nb_words-1; j>=0; --j)
	{
		if(set.get_word_id(sentence, num_sentence, static_cast<unsigned>(j)) == 0)
			continue;
		tmp.push_back(backward_lstm->add_input( 
				embedding.get_embedding_expr(cg, set.get_word_id(sentence, num_sentence, static_cast<unsigned>(j))) ) );
	}
	/* Concat */
	for(i=0; i<sentence_repr.size(); ++i)
	{
		vector<Expression> input_expr(2);
		input_expr[0] = sentence_repr[i];
		input_expr[1] = tmp[i];
		sentence_repr[i] = concatenate(input_expr);
	}

}

void BiLSTM::compute_alpha(ComputationGraph& cg, vector< vector<float> >& alpha_matrix, vector< vector<float> >& matrix)
{
	const unsigned premise_size = matrix.size();
	const unsigned hypothesis_size = matrix[0].size();
	float result=0;
	for(unsigned i=0; i<premise_size; ++i)
	{
		for(unsigned j=0; j<hypothesis_size; ++j)
		{
			result = 0;
			for(unsigned k=0; k<hypothesis_size; ++k)
					result += exp( matrix[i][k] );
			alpha_matrix[i][j] = exp( matrix[i][j] ) / result;
		}
	}
	/*Expression e = input(cg, Dim({premise_size, hypothesis_size}), alpha_matrix);
	return e;*/
}

void BiLSTM::compute_beta(ComputationGraph& cg, vector< vector<float> >& beta_matrix, vector< vector<float> >& matrix)
{
	const unsigned premise_size = matrix.size();
	const unsigned hypothesis_size = matrix[0].size();
	float result=0;
	for(unsigned i=0; i<premise_size; ++i)
	{
		for(unsigned j=0; j<hypothesis_size; ++j)
		{
			result = 0;
			for(unsigned k=0; k<premise_size; ++k)
				result += exp( matrix[k][j] );
			beta_matrix[i][j] = exp( matrix[i][j] ) / result;

			//cout << beta_matrix[i][j] << ' ';
		}
		//cout << endl;
	}
	/*Expression e = input(cg, {premise_size, hypothesis_size}, beta_matrix);
	return e;*/
}

void BiLSTM::create_attention_matrix(ComputationGraph& cg, vector< vector<float> >& matrix, 
	vector<Expression>& premise_lstm_repr, vector<Expression>& hypothesis_lstm_repr)
{
	const unsigned premise_size = premise_lstm_repr.size();
	const unsigned hypothesis_size = hypothesis_lstm_repr.size();
	for(unsigned i=0; i<premise_size; ++i)
	{
		for(unsigned j=0; j<hypothesis_size; ++j)
		{
			Expression e = transpose(premise_lstm_repr[i]) * hypothesis_lstm_repr[j];
			matrix[i][j] = as_scalar( e.value() );

			//cout << matrix[i][j] << ' ';
		}
		//cout << endl;
	}
}

void BiLSTM::compute_a_context_vector(ComputationGraph& cg, vector< vector<float> >& alpha_matrix, 
	vector<Expression>& hypothesis_lstm_repr, unsigned premise_size, vector<Expression>& a_c_vect)
{
	const unsigned hypothesis_size = hypothesis_lstm_repr.size();

	for(unsigned i=0; i<premise_size; ++i)
	{
		vector<Expression> vect;
		for(unsigned j=0; j<hypothesis_size; ++j)
		{
			Expression e = hypothesis_lstm_repr[j] * alpha_matrix[i][j];
			vect.push_back(e); 
		}
		a_c_vect[i] = sum(vect);
	}
}

void BiLSTM::compute_b_context_vector(ComputationGraph& cg, vector< vector<float> >& beta_matrix, vector<Expression>& premise_lstm_repr, 
	unsigned hypothesis_size, vector<Expression>& b_c_vect)
{
	const unsigned premise_size = premise_lstm_repr.size();

	for(unsigned j=0; j<hypothesis_size; ++j)
	{
		vector<Expression> vect;
		for(unsigned i=0; i<premise_size; ++i)
		{
			Expression e = premise_lstm_repr[i] * beta_matrix[i][j];
			vect.push_back(e); 
		}
		b_c_vect[j] = sum(vect);
	}
}

Expression BiLSTM::run_sys4(Data& set, Embeddings& embedding, unsigned num_sentence, ComputationGraph& cg)
{
	/* Representation of each word (of the premise and of the hypothesis)
	 * by the BiLSTM
	 */
	vector<Expression> premise_lstm_repr;
	vector<Expression> hypothesis_lstm_repr;
	words_representation(embedding, set, 1, cg, num_sentence, premise_lstm_repr);
	words_representation(embedding, set, 2, cg, num_sentence, hypothesis_lstm_repr);
	
	unsigned prem_size = premise_lstm_repr.size();
	unsigned hyp_size = hypothesis_lstm_repr.size();
	// Computing score 
	Expression W = parameter(cg, p_W); 
	Expression W_attention = parameter(cg, p_W_attention); // a rajouter constr
	Expression bias = parameter(cg, p_bias); 
	vector<Expression> scores;
	Expression mult;
	Expression tmp;
	vector< vector<float> > input(prem_size, vector<float>(hyp_size));
	unsigned i, j;
	for(i=0; i<prem_size; ++i)
	{
		mult = transpose(premise_lstm_repr[i]) * W_attention;
		for(j=0; j<hyp_size; ++j)
		{
			tmp = mult * hypothesis_lstm_repr[j];
			input[i][j] = tmp.value();
		}
	}	
	mult = input(cg, {prem_size, hyp_size}, input);
	Expression alpha = softmax(mult);
	//TODO softmax soi meme pour pas passer par des expressions
	
	
	//vector<vector<Expression>> s(prem_size, vector<float>(hyp_size));
	
	
	Expression score = affine_transform({bias, W, tmp};
	return score;
}

Expression BiLSTM::run_KIM(Data& set, Embeddings& embedding, unsigned num_sentence, ComputationGraph& cg) 
{
	/* Representation of each word (of the premise and of the hypothesis)
	 * by the BiLSTM explained in the step 1 of KIM.
	 */
	vector<Expression> premise_lstm_repr;
	vector<Expression> hypothesis_lstm_repr;
	words_representation(embedding, set, 1, cg, num_sentence, premise_lstm_repr);
	words_representation(embedding, set, 2, cg, num_sentence, hypothesis_lstm_repr);

	/* Creating attention matrix */
	vector< vector<float> > attention_matrix(premise_lstm_repr.size(), vector<float>(hypothesis_lstm_repr.size()) );
	create_attention_matrix(cg, attention_matrix, premise_lstm_repr, hypothesis_lstm_repr); 
	//cerr<< "Dim of attention matrix = ("<<attention_matrix.size()<<", "<< attention_matrix[0].size()<<")"<<endl;

	// Computing alpha and beta 
	vector< vector<float> > alpha_matrix(premise_lstm_repr.size(), vector<float>(hypothesis_lstm_repr.size()) );
	vector< vector<float> > beta_matrix(premise_lstm_repr.size(), vector<float>(hypothesis_lstm_repr.size()) );
	compute_alpha(cg, alpha_matrix, attention_matrix);
	compute_beta(cg, beta_matrix, attention_matrix); //softmax(e_ij);
	/*cerr<< "Dim of alpha matrix = ("<<alpha_matrix.size()<<", "<< alpha_matrix[0].size()<<")"<<endl;
	cerr<< "Dim of beta matrix = ("<<beta_matrix.size()<<", "<< beta_matrix[0].size()<<")"<<endl;*/

	// Computing context-vector 
	vector<Expression> a_c_vect(premise_lstm_repr.size());
	vector<Expression> b_c_vect(hypothesis_lstm_repr.size());
	compute_a_context_vector(cg, alpha_matrix, hypothesis_lstm_repr, premise_lstm_repr.size(), a_c_vect);
	compute_b_context_vector(cg, beta_matrix, premise_lstm_repr, hypothesis_lstm_repr.size(), b_c_vect);
	/*cerr<< "Dim of a context vector = "<<a_c_vect.size()<<endl;
	cerr<< "Dim of b context vector = "<<b_c_vect.size()<<endl;*/

	// Pooling 
	vector<Expression> pool(2);
	pool[0] = sum(a_c_vect) / static_cast<double>(premise_lstm_repr.size());
	pool[1] = sum(b_c_vect) / static_cast<double>(hypothesis_lstm_repr.size());

	// Concat pooling 
	Expression concat = concatenate(pool);
	//cerr<< "Dim of the final vector = "<<concat.dim()<<endl;

	// Computing score 
	Expression W = parameter(cg, p_W); 
	Expression bias = parameter(cg, p_bias); 
	Expression score = affine_transform({bias, W, concat});

	return score;
}
