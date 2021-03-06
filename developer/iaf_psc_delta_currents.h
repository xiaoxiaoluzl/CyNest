/*
 *  iaf_psc_delta_currents.h
 *
 *  This file is part of NEST
 *
 *  Copyright (C) 2004 by
 *  The NEST Initiative
 *
 *  See the file AUTHORS for details.
 *
 *  Permission is granted to compile and modify
 *  this file for non-commercial use.
 *  See the file LICENSE for details.
 *
 */

#ifndef IAF_PSC_DELTA_CURRENTS_H
#define IAF_PSC_DELTA_CURRENTS_H

#include "nest.h"
#include "event.h"
#include "archiving_node.h"
#include "ring_buffer.h"
#include "connection.h"
#include "analog_data_logger.h"

namespace nest{
  
  class Network;

  /* BeginDocumentation
     Name: iaf_psc_delta - Leaky integrate-and-fire neuron model.

     Description:

     iaf_psc_delta is an implementation of a leaky integrate-and-fire model
     where the potential jumps on each spike arrival. 

     The threshold crossing is followed by an absolute refractory period
     during which the membrane potential is clamped to the resting potential.

     Spikes arriving while the neuron is refractory, are discarded by
     default. If the property "refractory_input" is set to true, such
     spikes are added to the membrane potential at the end of the
     refractory period, dampened according to the interval between
     arrival and end of refractoriness.

     The linear subthresold dynamics is integrated by the Exact
     Integration scheme [1]. The neuron dynamics is solved on the time
     grid given by the computation step size. Incoming as well as emitted
     spikes are forced to that grid.  

     An additional state variable and the corresponding differential
     equation represents a piecewise constant external current.

     The general framework for the consistent formulation of systems with
     neuron like dynamics interacting by point events is described in
     [1].  A flow chart can be found in [2].

     Critical tests for the formulation of the neuron model are the
     comparisons of simulation results for different computation step
     sizes. sli/testsuite/nest contains a number of such tests.
  
     The iaf_psc_delta is the standard model used to check the consistency
     of the nest simulation kernel because it is at the same time complex
     enough to exhibit non-trivial dynamics and simple enough compute
     relevant measures analytically.

     Remarks:

     The present implementation uses individual variables for the
     components of the state vector and the non-zero matrix elements of
     the propagator.  Because the propagator is a lower triangular matrix
     no full matrix multiplication needs to be carried out and the
     computation can be done "in place" i.e. no temporary state vector
     object is required.

     The template support of recent C++ compilers enables a more succinct
     formulation without loss of runtime performance already at minimal
     optimization levels. A future version of iaf_psc_delta will probably
     address the problem of efficient usage of appropriate vector and
     matrix objects.


     Parameters: 

     The following parameters can be set in the status dictionary.

     V_m        double - Membrane potential in mV 
     E_L        double - Resting membrane potential in mV. 
     c_m        double - Specific capacitance of the membrane in pF/mum^2 
     tau_m      double - Membrane time constant in ms. 
     t_ref      double - Duration of refractory period in ms. 
     V_th       double - Spike threshold in mV. 
     V_reset    double - Reset potential of the membrane in mV.
     I_e        double - Constant input current in pA. 
     V_min      double - Absolute lower value for the membrane potential.

     refractory_input bool - If true, do not discard input during
     refractory period. Default: false.
 
     References:
     [1] Rotter S & Diesmann M (1999) Exact simulation of time-invariant linear
     systems with applications to neuronal modeling. Biologial Cybernetics
     81:381-402.
     [2] Diesmann M, Gewaltig M-O, Rotter S, & Aertsen A (2001) State space 
     analysis of synchronous spiking in cortical neural networks. 
     Neurocomputing 38-40:565-571.


     Author:  September 1999, Diesmann, Gewaltig
     SeeAlso: iaf_cond_neuron, testsuite::test_iaf, lifb_cond_neuron, iaf_rev_neuron
  */

  /**
   * Leaky integrate-and-fire neuron with delta-shaped PSCs.
   */
  class iaf_psc_delta_currents:
  public Archiving_Node
  {
    
  public:        
    
    typedef Node base;
    
    iaf_psc_delta_currents();
    iaf_psc_delta_currents(const iaf_psc_delta_currents&);

    /**
     * Import sets of overloaded virtual functions.
     * We need to explicitly include sets of overloaded
     * virtual functions into the current scope.
     * According to the SUN C++ FAQ, this is the correct
     * way of doing things, although all other compilers
     * happily live without.
     */

    using Node::connect_sender;
    using Node::handle;

    port check_connection(Connection&, port);
    
    void handle(SpikeEvent &);
    void handle(CurrentEvent &);
    void handle(PotentialRequest &);
    void handle(SynapticCurrentRequest &);
    
    port connect_sender(SpikeEvent &, port);
    port connect_sender(CurrentEvent &, port);
    port connect_sender(PotentialRequest &, port);
    port connect_sender(SynapticCurrentRequest &, port);

    void get_status(DictionaryDatum &) const;
    void set_status(const DictionaryDatum &);

  private:

    void init_state_(const Node& proto);
    void init_buffers_();
    void calibrate();

    void update(Time const &, const long_t, const long_t);
    
    // ---------------------------------------------------------------- 

    /** 
     * Independent parameters of the model. 
     */
    struct Parameters_ {
      /** Membrane time constant in ms. */
      double_t tau_m_;

      /** Membrane capacitance in pF. */
      double_t c_m_;
    
      /** Refractory period in ms. */
      double_t t_ref_;

      /** Resting potential in mV. */
      double_t E_L_;

      /** External DC current */
      double_t I_e_;

      /** Threshold, RELATIVE TO RESTING POTENTAIL(!).
          I.e. the real threshold is (U0_+V_th_). */
      double_t V_th_;

      /** Lower bound, RELATIVE TO RESTING POTENTAIL(!).
          I.e. the real lower bound is (V_min_+V_th_). */
      double_t V_min_;

      /** reset value of the membrane potential */
      double_t V_reset_;

      bool     with_refr_input_;   //!< spikes arriving during refractory period are counted

      Parameters_();  //!< Sets default parameter values

      void get(DictionaryDatum&) const;  //!< Store current values in dictionary
      void set(const DictionaryDatum&);  //!< Set values from dicitonary
    };
    
    // ---------------------------------------------------------------- 

    /**
     * State variables of the model.
     */
    struct State_ {
      double_t     y0_;
      double_t     y3_; //!< This is the membrane potential RELATIVE TO RESTING POTENTIAL.

      int_t    r_;  //!< Number of refractory steps remaining

      /** Accumulate spikes arriving during refractory period, discounted for
	  decay until end of refractory period.
      */
      double_t    refr_spikes_buffer_;

      State_();  //!< Default initialization
      
      void get(DictionaryDatum&, const Parameters_&) const;
      void set(const DictionaryDatum&, const Parameters_&);
    };    

    // ---------------------------------------------------------------- 

    /**
     * Buffers of the model.
     */
    struct Buffers_ {
      /** buffers and summs up incoming spikes/currents */
      RingBuffer spikes_;
      RingBuffer currents_;  

      /**
       * Buffer for membrane potential.
       */
      AnalogDataLogger<PotentialRequest> potentials_;

      /** 
       * Buffer for the currents.
       */
      AnalogDataLogger<SynapticCurrentRequest> syncurrents_;
    };
    
    // ---------------------------------------------------------------- 

    /**
     * Internal variables of the model.
     */
    struct Variables_ { 
    
      double_t P30_;
      double_t P33_;  

      int_t       RefractoryCounts_;

    };

    // ---------------------------------------------------------------- 

    /**
     * @defgroup iaf_psc_alpha_data
     * Instances of private data structures for the different types
     * of data pertaining to the model.
     * @note The order of definitions is important for speed.
     * @{
     */   
    Parameters_ P_;
    State_      S_;
    Variables_  V_;
    Buffers_    B_;
    /** @} */
    
  };
    
  inline
    port iaf_psc_delta_currents::check_connection(Connection& c, port receptor_type)
    {
      SpikeEvent e;
      e.set_sender(*this);
      c.check_event(e);
      return c.get_target()->connect_sender(e, receptor_type);
    }

  inline
    port iaf_psc_delta_currents::connect_sender(SpikeEvent&, port receptor_type)
    {
      if (receptor_type != 0)
	throw UnknownReceptorType(receptor_type, get_name());
      return 0;
    }
 
  inline
    port iaf_psc_delta_currents::connect_sender(CurrentEvent&, port receptor_type)
    {
      if (receptor_type != 0)
	throw UnknownReceptorType(receptor_type, get_name());
      return 0;
    }
 
  inline
    port iaf_psc_delta_currents::connect_sender(PotentialRequest& pr, port receptor_type)
    {
      if (receptor_type != 0)
	throw UnknownReceptorType(receptor_type, get_name());
      B_.potentials_.connect_logging_device(pr);
      return 0;
    }

  inline
    port iaf_psc_delta_currents::connect_sender(SynapticCurrentRequest& pr, port receptor_type)
    {
      if (receptor_type != 0)
	throw UnknownReceptorType(receptor_type, get_name());
      B_.syncurrents_.connect_logging_device(pr);
      return 0;
    }



inline
void iaf_psc_delta_currents::get_status(DictionaryDatum &d) const
{
  P_.get(d);
  S_.get(d, P_);
  Archiving_Node::get_status(d);
}

inline
void iaf_psc_delta_currents::set_status(const DictionaryDatum &d)
{
  Parameters_ ptmp = P_;  // temporary copy in case of errors
  ptmp.set(d);                       // throws if BadProperty
  State_      stmp = S_;  // temporary copy in case of errors
  stmp.set(d, ptmp);                 // throws if BadProperty

  // We now know that (ptmp, stmp) are consistent. We do not 
  // write them back to (P_, S_) before we are also sure that 
  // the properties to be set in the parent class are internally 
  // consistent.
  Archiving_Node::set_status(d);

  // if we get here, temporaries contain consistent set of properties
  P_ = ptmp;
  S_ = stmp;
}

} // namespace

#endif /* #ifndef IAF_PSC_DELTA_H */

