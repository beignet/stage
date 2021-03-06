#ifndef LSTM_HPP
#define LSTM_HPP

#include "../code_data/dataset.hpp"
#include "rnn.hpp"

/**
 * \file LSTM.hpp
*/

/** 
 * \class LSTM
 * \brief Class representing a LSTM. Herits from the RNN class.
*/	
class LSTM : public RNN
{
	private:
		dynet::Expression sentence_representation(DataSet& set, 
												  Embeddings& embedding, 
												  bool is_premise, 
												  unsigned num_sentence, 
												  dynet::ComputationGraph& cg);
												  
		dynet::Expression run(DataSet& set, 
							  Embeddings& embedding, 
							  unsigned num_sentence, 
							  dynet::ComputationGraph& cg);
							  
							  
		dynet::Expression systeme_1(std::vector<dynet::Expression>& h, 
									dynet::Expression bias, 
									dynet::Expression W);
									
									
		dynet::Expression systeme_2(std::vector<dynet::Expression>& h, 
									dynet::Expression bias, 
									dynet::Expression W);
									
									
		virtual std::vector<float> predict(DataSet& set, 
										   Embeddings& embedding, 
										   unsigned num_sentence, 
										   dynet::ComputationGraph& cg, 
										   bool print_proba, 
										   unsigned& argmax);
										   
										   
		virtual dynet::Expression get_neg_log_softmax(DataSet& set, 
													  Embeddings& embedding, 
													  unsigned num_sentence, 
													  dynet::ComputationGraph& cg);


	public:
		LSTM(unsigned nblayer, 
			 unsigned inputdim, 
			 unsigned hiddendim, 
			 float dropout, 
			 unsigned s, 
			 dynet::ParameterCollection& model,
			 bool original_lime);
};

#endif
