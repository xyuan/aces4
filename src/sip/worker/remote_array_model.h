/*
 * remote_array_model.h
 *
 *  Created on: Oct 29, 2014
 *      Author: njindal
 */

#ifndef REMOTE_ARRAY_MODEL_H_
#define REMOTE_ARRAY_MODEL_H_

#include "data_distribution.h"

namespace sip {

class BlockId;
class SipTables;
class SIPMPIAttr;

class RemoteArrayModel {
public:

	struct Parameters {
		double interconnect_alpha;		//! Startup time in seconds (time = alpha + beta*block_size)
		double interconnect_beta;		//! Bandwidth in bytes per second
		double daxpy_alpha;				//! DAXPY Time = alpha + beta * block_size
		double daxpy_beta;
		Parameters(): interconnect_alpha(-1.0), interconnect_beta(-1.0), daxpy_alpha(-1.0), daxpy_beta(-1.0) {};
		Parameters(const Parameters& other) :
				interconnect_alpha(other.interconnect_alpha), interconnect_beta(other.interconnect_beta),
				daxpy_alpha(other.daxpy_alpha), daxpy_beta(other.daxpy_beta){
		}
	};

	RemoteArrayModel(const SipTables& sip_tables, const Parameters& parameters);
	~RemoteArrayModel();

	/** @return Time it takes to send a small message to the server	 */
	double time_to_send_small_message_to_server() const { return parameters_.interconnect_alpha; }

	/** @return Time it takes to send the ack from the server to the worker */
	double time_to_get_ack_from_server() const { return parameters_.interconnect_alpha; }

	/** @return Time to send a block synchronously to the server */
	double time_to_send_block_to_server(const BlockId& block_id) const;

	/** @return Time to GET/PREPARE a block at the server */
	double time_to_get_block(const BlockId& block_id) const ;

	/** @return Time to process PUT/PREPARE REPLACE at the server */
	double time_to_put_replace_block(const BlockId& block_id) const ;

	/** @return Time to process PUT/PREPARE SUM at the server */
	double time_to_put_accumulate_block(const BlockId& block_id) const;

	/** @return Time to create an array at the server */
	double time_to_create_array(int array_id) const { return 0.0; }	// No-op in current implementation

	/** @return Time to delete an array at the server */
	double  time_to_delete_array(int array_id) const  { return 0.0; } // Not accurately modeled

	/** @return Time to mark an array persistent */
	double time_to_set_persistent(int array_id) const { return 0.0; } // Just an insert-into-map operation

	/** @return Time to restore a previously marked persistent array */
	double time_to_restore_persistent(int array_id) const { return 0.0; } // Not accurately modeled

private:
	const SipTables& sip_tables_;
	const Parameters& parameters_;
};

} /* namespace sip */

#endif /* REMOTE_ARRAY_MODEL_H_ */