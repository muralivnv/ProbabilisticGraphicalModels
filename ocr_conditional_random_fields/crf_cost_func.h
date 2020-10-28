#ifndef _CRF_COST_FUNC_H_
#define _CRF_COST_FUNC_H_

#include "crf_typedef.h"
#include "crf_inference.h"

float log_conditional_prob(const crf::Word_t&          feature_seq,
                           const vector<size_t>&       label_seq,
                           const crf::NodeWeights_t&   node_weights, 
                           const crf::TransWeights_t&  transition_weights,
                           const crf::Graph&           graph)
{
  (void)node_weights;
  
  float cost = 0.0F;

  // for the first node at time t = 0u
  cost += graph.WX(label_seq[0u], 0);
  for (size_t i = 1u; i < feature_seq.size(); i++)
  {
    cost += graph.WX(label_seq[i], i);
    cost += transition_weights(label_seq[i-1], label_seq[i]);
  }
  cost *= -1.0F;
  
  // sum up partition function
  cost += graph.log_alpha(all, last).sum(); 

  return cost;
}


// TODO: remove unnecessary calculations for PY2Y1X
// TODO: check calculations to make sure everything is correct or not
// TODO: check why cost is keep going up
void log_conditional_prime(const crf::Word_t&          feature_seq,
                           const vector<size_t>&       label_seq,
                           const crf::NodeWeights_t&   node_weights, 
                           const crf::TransWeights_t&  transition_weights,
                           const crf::Graph&           graph,
                                 crf::MatrixX<float>&  gradient_out)
{
  const size_t seq_len            = feature_seq.size();
  const size_t n_states           = node_weights.rows();
  const size_t feature_len        = feature_seq[0].size();
  const size_t trans_weight_start = node_weights.size();

  // node gradient
  for (size_t t = 0u; t < seq_len; t++)
  {
    size_t label = label_seq[t];

    float normalized_PY1X = graph.unnormalized_PY1X(label, t)/graph.unnormalized_PY1X(all, t).sum();
    float scaling = normalized_PY1X - 1.0F;

    // Fill gradient
    gradient_out(seq(label*feature_len, (label+1)*feature_len-1u), 0).noalias() += scaling*feature_seq[t];
  }

  // transition gradient
  for (size_t t = 1u; t < seq_len; t++)
  {
    size_t label_prev = label_seq[t-1u];
    size_t label_now  = label_seq[t];

    float numerator = graph.WX(label_prev, t-1u);
    numerator      += graph.WX(label_now, t);
    numerator      += transition_weights(label_prev, label_now);

    if (t > 1u) // add forward pass result
    { numerator += std::logf(graph.log_alpha(label_prev, t-2u)); }

    if (t < seq_len - 1u) // add backward pass result
    { numerator += std::logf(graph.log_beta(label_now, t+1u)); }
    numerator = std::expf(numerator);

    float denominator = 0.0F;
    for (size_t state_i = 0u; state_i < n_states; state_i++)
    {
      float state_i_potential = graph.WX(state_i, t-1u);
      for (size_t state_j = 0u; state_j < n_states; state_j++)
      {
        float coeff = graph.WX(state_j, t);
        coeff += state_i_potential;

        if (t > 1u)
        { coeff += std::logf(graph.log_alpha(state_i, t-2u)); }
        if (t < seq_len-1u)
        { coeff += std::logf(graph.log_beta(state_j, t+1u)); }

        denominator += std::expf(coeff);
      }
    }

    gradient_out(trans_weight_start + (label_prev*n_states) + label_now) += ((numerator / denominator) - 1.0F);
  }
}

#endif