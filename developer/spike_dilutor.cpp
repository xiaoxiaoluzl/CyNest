/*
 *  spike_dilutor.cpp
 *
 *  This file is part of NEST
 *
 *  Copyright (C) 2011 by
 *  The NEST Initiative
 *
 *  See the file AUTHORS for details.
 *
 *  Permission is granted to compile and modify
 *  this file for non-commercial use.
 *  See the file LICENSE for details.
 *
 */

#include "spike_dilutor.h"
#include "network.h"
#include "dict.h"
#include "random_datums.h"
#include "dictutils.h"
#include "exceptions.h"
#include "gslrandomgen.h"

/* ----------------------------------------------------------------
 * Default constructors defining default parameter
 * ---------------------------------------------------------------- */

nest::spike_dilutor::Parameters_::Parameters_()
  : p_copy_(1.0)
{}

nest::spike_dilutor::Parameters_::Parameters_(const Parameters_ & p)
  : p_copy_(p.p_copy_)
{}

/* ----------------------------------------------------------------
 * Parameter extraction and manipulation functions
 * ---------------------------------------------------------------- */

void nest::spike_dilutor::Parameters_::get(DictionaryDatum & d) const
{
  (*d)[names::p_copy] = p_copy_;
}

void nest::spike_dilutor::Parameters_::set(const DictionaryDatum & d)
{
  updateValue<double_t>(d, names::p_copy, p_copy_);

  if ( p_copy_ < 0 || p_copy_ > 1 )
    throw BadProperty("Copy probability must be in [0, 1].");
}

/* ----------------------------------------------------------------
 * Default and copy constructor for node
 * ---------------------------------------------------------------- */

nest::spike_dilutor::spike_dilutor()
  : Node(),
    device_(),
    P_()
{}

nest::spike_dilutor::spike_dilutor(const spike_dilutor & n)
  : Node(n),
    device_(n.device_),
    P_(n.P_)
{}

/* ----------------------------------------------------------------
 * Node initialization functions
 * ---------------------------------------------------------------- */

void nest::spike_dilutor::init_state_(const Node & proto)
{
  const spike_dilutor& pr = downcast<spike_dilutor>(proto);

  device_.init_state(pr.device_);
}

void nest::spike_dilutor::init_buffers_()
{
  B_.n_spikes_.clear();  // includes resize
  device_.init_buffers();
}

void nest::spike_dilutor::calibrate()
{
  device_.calibrate();
}

/* ----------------------------------------------------------------
 * Other functions
 * ---------------------------------------------------------------- */

void nest::spike_dilutor::update(Time const & T, const long_t from, const long_t to)
{
  assert (to >= 0 && (delay) from < Scheduler::get_min_delay() );
  assert( from < to );

  for ( long_t lag = from ; lag < to ; ++lag )
  {
    if ( !device_.is_active(T) )
      return; // no spikes to be repeated

    // generate spikes of mother process for each time slice
    ulong_t n_mother_spikes = static_cast<ulong_t>(B_.n_spikes_.get_value(lag));

    if ( n_mother_spikes )
    {
      DSSpikeEvent se;

      se.set_multiplicity(n_mother_spikes);
      network()->send(*this, se, lag);
    }
  }
}

void nest::spike_dilutor::event_hook(DSSpikeEvent& e)
{
  // note: event_hook() receives a reference of the spike event that
  // was originally created in the update function. there we set
  // the multiplicty to store the number of mother spikes. the *same*
  // reference will be delivered multiple times to the event hook,
  // once for every receiver. when calling handle() of the receiver
  // above, we need to change the multiplicty to the number of copied
  // child process spikes, so afterwards it needs to be reset to correctly
  // store the number of mother spikes again during the next call of event_hook().
  // reichert

  librandom::RngPtr rng = net_->get_rng(get_thread());
  ulong_t n_mother_spikes = e.get_multiplicity();
  ulong_t n_spikes = 0;

  for ( ulong_t n = 0; n < n_mother_spikes; n++ )
  {
    if ( rng->drand() < P_.p_copy_ )
      n_spikes++;
  }

  if ( n_spikes > 0 )
  {
    e.set_multiplicity(n_spikes);
    e.get_receiver().handle(e);
  }

  e.set_multiplicity(n_mother_spikes);
}

void nest::spike_dilutor::handle(SpikeEvent & e)
{
  B_.n_spikes_.add_value(e.get_rel_delivery_steps(network()->get_slice_origin()),
                         static_cast<double_t>(e.get_multiplicity()));
}
