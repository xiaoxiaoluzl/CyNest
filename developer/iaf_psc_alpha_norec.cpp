/*
 *  iaf_psc_alpha_norec.cpp
 *
 *  This file is part of NEST
 *
 *  Copyright (C) 2004-2008 by
 *  The NEST Initiative
 *
 *  See the file AUTHORS for details.
 *
 *  Permission is granted to compile and modify
 *  this file for non-commercial use.
 *  See the file LICENSE for details.
 *
 */

#include "exceptions.h"
#include "iaf_psc_alpha_norec.h"
#include "network.h"
#include "dict.h"
#include "integerdatum.h"
#include "doubledatum.h"
#include "dictutils.h"
#include "numerics.h"

#include <limits>


namespace nest
{

/* ---------------------------------------------------------------- 
 * Default constructors defining default parameters and state
 * ---------------------------------------------------------------- */
    
nest::iaf_psc_alpha_norec::Parameters_::Parameters_()
  : Tau_       ( 10.0    ),  // ms
    C_         (250.0    ),  // pF
    TauR_      (  2.0    ),  // ms
    U0_        (-70.0    ),  // mV
    I_e_       (  0.0    ),  // pA
    V_reset_   (-70.0-U0_),  // mV, rel to U0_
    Theta_     (-55.0-U0_),  // mV, rel to U0_
    LowerBound_(-std::numeric_limits<double_t>::infinity()),
    tau_ex_    (  2.0    ),  // ms
    tau_in_    (  2.0    )   // ms
{
}

nest::iaf_psc_alpha_norec::State_::State_()
  : y0_   (0.0),
    y1_ex_(0.0),
    y2_ex_(0.0),
    y1_in_(0.0),
    y2_in_(0.0),
    y3_   (0.0),  
    r_    (0)
{}

/* ---------------------------------------------------------------- 
 * Parameter and state extractions and manipulation functions
 * ---------------------------------------------------------------- */

void nest::iaf_psc_alpha_norec::Parameters_::get(DictionaryDatum &d) const
{
  def<double>(d, names::E_L, U0_);   // Resting potential
  def<double>(d, names::I_e, I_e_);
  def<double>(d, names::V_th, Theta_+U0_); // threshold value
  def<double>(d, names::V_reset, V_reset_+U0_);
  def<double>(d, names::V_min, LowerBound_+U0_);
  def<double>(d, names::C_m, C_);
  def<double>(d, names::tau_m, Tau_);
  def<double>(d, names::tau_syn_ex, tau_ex_);
  def<double>(d, names::tau_syn_in, tau_in_);
  def<double>(d, names::t_ref, TauR_);
}

double nest::iaf_psc_alpha_norec::Parameters_::set(const DictionaryDatum& d)
{
  // if U0_ is changed, we need to adjust all variables defined relative to U0_
  const double ELold = U0_;
  updateValue<double>(d, names::E_L, U0_);
  const double delta_EL = U0_ - ELold;

  if(updateValue<double>(d, names::V_reset, V_reset_))
    V_reset_ -= U0_;
  else
    V_reset_ -= delta_EL;

  if (updateValue<double>(d, names::V_th, Theta_)) 
    Theta_ -= U0_;
  else
    Theta_ -= delta_EL;

  if (updateValue<double>(d, names::V_min, LowerBound_))
    LowerBound_ -= U0_;
  else
    LowerBound_ -= delta_EL;
    
  updateValue<double>(d, names::I_e, I_e_);
  if ( C_ <= 0. )
    throw BadProperty("Capacitance must be strictly positive.");

  updateValue<double>(d, names::C_m, C_);

  if ( Tau_ <= 0. )
    throw BadProperty("Membrane time constant must be > 0.");
  updateValue<double>(d, names::tau_m, Tau_);

  if (tau_ex_ <= 0. || tau_in_ <= 0 )
    throw BadProperty("All synaptic time constants must be > 0.");
  updateValue<double>(d, names::tau_syn_ex, tau_ex_);
  updateValue<double>(d, names::tau_syn_in, tau_in_);

  if ( TauR_ < 0. )
  	throw BadProperty("The refractory time t_ref can't be negative.");

  updateValue<double>(d, names::t_ref, TauR_);

  if ( V_reset_ >= Theta_ )
    throw BadProperty("Reset potential must be smaller than threshold.");

  return delta_EL;
}

void nest::iaf_psc_alpha_norec::State_::get(DictionaryDatum &d, const Parameters_& p) const
{
  def<double>(d, names::V_m, y3_ + p.U0_); // Membrane potential
}

void nest::iaf_psc_alpha_norec::State_::set(const DictionaryDatum& d, const Parameters_& p, double delta_EL)
{
  if ( updateValue<double>(d, names::V_m, y3_) )
    y3_ -= p.U0_;
  else
    y3_ -= delta_EL;
}

nest::iaf_psc_alpha_norec::Buffers_::Buffers_(iaf_psc_alpha_norec&)
{}

nest::iaf_psc_alpha_norec::Buffers_::Buffers_(const Buffers_&, iaf_psc_alpha_norec&)
{}


/* ---------------------------------------------------------------- 
 * Default and copy constructor for node
 * ---------------------------------------------------------------- */

nest::iaf_psc_alpha_norec::iaf_psc_alpha_norec()
  : Archiving_Node(), 
    P_(), 
    S_(),
    B_(*this)
{
}

nest::iaf_psc_alpha_norec::iaf_psc_alpha_norec(const iaf_psc_alpha_norec& n)
  : Archiving_Node(n), 
    P_(n.P_), 
    S_(n.S_),
    B_(n.B_, *this)
{}

/* ---------------------------------------------------------------- 
 * Node initialization functions
 * ---------------------------------------------------------------- */

void nest::iaf_psc_alpha_norec::init_state_(const Node& proto)
{
  const iaf_psc_alpha_norec& pr = downcast<iaf_psc_alpha_norec>(proto);
  S_ = pr.S_;
}

void nest::iaf_psc_alpha_norec::init_buffers_()
{
  B_.ex_spikes_.clear();       // includes resize
  B_.in_spikes_.clear();       // includes resize
  B_.currents_.clear();        // includes resize

  Archiving_Node::clear_history();
}

void nest::iaf_psc_alpha_norec::calibrate()
{
  const double h = Time::get_resolution().get_ms(); 

  // these P are independent
  V_.P11_ex_ = V_.P22_ex_ = std::exp(-h/P_.tau_ex_);
  V_.P11_in_ = V_.P22_in_ = std::exp(-h/P_.tau_in_);

  V_.P33_ = std::exp(-h/P_.Tau_);

  V_.expm1_tau_m_ = numerics::expm1(-h/P_.Tau_);

  // these depend on the above. Please do not change the order.
  V_.P30_ = -P_.Tau_/P_.C_*numerics::expm1(-h/P_.Tau_);

  V_.P21_ex_ = h * V_.P11_ex_;
  V_.P31_ex_ = 1/P_.C_ * ((V_.P11_ex_-V_.P33_)/(-1/P_.tau_ex_- -1/P_.Tau_)- h*V_.P11_ex_)
    /(-1/P_.Tau_ - -1/P_.tau_ex_);
  V_.P32_ex_ = 1/P_.C_*(V_.P33_-V_.P11_ex_)/(-1/P_.Tau_ - -1/P_.tau_ex_);

  V_.P21_in_ = h * V_.P11_in_;
  V_.P31_in_ = 1/P_.C_ * ((V_.P11_in_-V_.P33_)/(-1/P_.tau_in_- -1/P_.Tau_)- h*V_.P11_in_)
    /(-1/P_.Tau_ - -1/P_.tau_in_);
  V_.P32_in_ = 1/P_.C_*(V_.P33_-V_.P11_in_)/(-1/P_.Tau_ - -1/P_.tau_in_);

  V_.EPSCInitialValue_=1.0 * numerics::e/P_.tau_ex_;
  V_.IPSCInitialValue_=1.0 * numerics::e/P_.tau_in_;

  // TauR specifies the length of the absolute refractory period as 
  // a double_t in ms. The grid based iaf_psc_alpha_norec can only handle refractory
  // periods that are integer multiples of the computation step size (h).
  // To ensure consistency with the overall simulation scheme such conversion
  // should be carried out via objects of class nest::Time. The conversion 
  // requires 2 steps:
  //     1. A time object is constructed defining representation of 
  //        TauR in tics. This representation is then converted to computation time
  //        steps again by a strategy defined by class nest::Time.
  //     2. The refractory time in units of steps is read out get_steps(), a member
  //        function of class nest::Time.
  //
  // The definition of the refractory period of the iaf_psc_alpha_norec is consistent 
  // the one of iaf_psc_alpha_norec_ps.
  //
  // Choosing a TauR that is not an integer multiple of the computation time 
  // step h will lead to accurate (up to the resolution h) and self-consistent
  // results. However, a neuron model capable of operating with real valued spike
  // time may exhibit a different effective refractory time.

  V_.RefractoryCounts_ = Time(Time::ms(P_.TauR_)).get_steps();
  assert(V_.RefractoryCounts_ >= 0);  // since t_ref_ >= 0, this can only fail in error
}

/* ---------------------------------------------------------------- 
 * Update and spike handling functions
 */
 
void nest::iaf_psc_alpha_norec::update(Time const & origin, const long_t from, const long_t to)
{
  assert(to >= 0 && (delay) from < Scheduler::get_min_delay());
  assert(from < to);

  for ( long_t lag = from ; lag < to ; ++lag )
  {
    if ( S_.r_ == 0 )
    {
      // neuron not refractory
      S_.y3_ = V_.P30_*(S_.y0_ + P_.I_e_) 
               + V_.P31_ex_ * S_.y1_ex_ + V_.P32_ex_ * S_.y2_ex_ 
               + V_.P31_in_ * S_.y1_in_ + V_.P32_in_ * S_.y2_in_
               + V_.expm1_tau_m_ * S_.y3_ + S_.y3_;

      // lower bound of membrane potential
      S_.y3_ = ( S_.y3_ < P_.LowerBound_ ? P_.LowerBound_ : S_.y3_); 
    }
    else // neuron is absolute refractory
     --S_.r_;

    // alpha shape EPSCs
    S_.y2_ex_  = V_.P21_ex_ * S_.y1_ex_ + V_.P22_ex_ * S_.y2_ex_;
    S_.y1_ex_ *= V_.P11_ex_;

    // Apply spikes delivered in this step; spikes arriving at T+1 have 
    // an immediate effect on the state of the neuron
    V_.weighted_spikes_ex_ = B_.ex_spikes_.get_value(lag);
    S_.y1_ex_ += V_.EPSCInitialValue_ * V_.weighted_spikes_ex_;

    // alpha shape EPSCs
    S_.y2_in_  = V_.P21_in_ * S_.y1_in_ + V_.P22_in_ * S_.y2_in_;
    S_.y1_in_ *= V_.P11_in_;

    // Apply spikes delivered in this step; spikes arriving at T+1 have 
    // an immediate effect on the state of the neuron
    V_.weighted_spikes_in_ = B_.in_spikes_.get_value(lag);
    S_.y1_in_ += V_.IPSCInitialValue_ * V_.weighted_spikes_in_;

    // threshold crossing
    if ( S_.y3_ >= P_.Theta_)
    {
      S_.r_  = V_.RefractoryCounts_;
      S_.y3_ = P_.V_reset_; 
      // A supra-threshold membrane potential should never be observable.
      // The reset at the time of threshold crossing enables accurate integration
      // independent of the computation step size, see [2,3] for details.   

      set_spiketime(Time::step(origin.get_steps()+lag+1));
      SpikeEvent se;
      network()->send(*this, se, lag);
    }

    // set new input current
    S_.y0_ = B_.currents_.get_value(lag);
  }  
}                           
                     

void nest::iaf_psc_alpha_norec::handle(SpikeEvent & e)
{
  assert(e.get_delay() > 0);

  if(e.get_weight() > 0.0)
    B_.ex_spikes_.add_value(e.get_rel_delivery_steps(network()->get_slice_origin()),
                            e.get_weight() * e.get_multiplicity() );
  else    
    B_.in_spikes_.add_value(e.get_rel_delivery_steps(network()->get_slice_origin()),
                            e.get_weight() * e.get_multiplicity() );
}

void nest::iaf_psc_alpha_norec::handle(CurrentEvent& e)
{
  assert(e.get_delay() > 0);

  const double_t I = e.get_current();
  const double_t w = e.get_weight();

  // add weighted current; HEP 2002-10-04
  B_.currents_.add_value(e.get_rel_delivery_steps(network()->get_slice_origin()), 
		                     w * I);
}

} // namespace
