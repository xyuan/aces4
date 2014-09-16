/** This test case exercises sial language features that
 * are not local.
 */

#include "gtest/gtest.h"
#include <fenv.h>
#include <execinfo.h>
#include <signal.h>
#include <cstdlib>
#include <cassert>
#include "siox_reader.h"
#include "io_utils.h"
#include "setup_reader.h"

#include "sip_tables.h"
#include "interpreter.h"
#include "setup_interface.h"
#include "sip_interface.h"
#include "data_manager.h"
#include "global_state.h"
#include "sial_printer.h"

#include "worker_persistent_array_manager.h"

#include "block.h"

#ifdef HAVE_TAU
#include <TAU.h>
#endif

#ifdef HAVE_MPI
#include "sip_server.h"
#include "server_persistent_array_manager.h"
//#include "sip_mpi_attr.h"
//#include "global_state.h"
//#include "sip_mpi_utils.h"
//#else
//#include "sip_attr.h"
#endif

#include "test_constants.h"
#include "test_controller.h"
#include "test_controller_parallel.h"

extern "C" {
int test_transpose_op(double*);
int test_transpose4d_op(double*);
int test_contraction_small2(double*);
}

//bool VERBOSE_TEST = false;
bool VERBOSE_TEST = true;


TEST(Sial,empty){
	std::string job("empty");
	int norb = 3;
        int segs[] = {2,2,2};
	if (attr->global_rank() == 0) {
		init_setup(job.c_str());
		std::string tmp = job + ".siox";
		const char* nm = tmp.c_str();
		add_sial_program(nm);
		set_aoindex_info(3, segs);
		finalize_setup();
	}
	std::stringstream output;

	TestControllerParallel controller(job, true, VERBOSE_TEST, "", output);
	controller.initSipTables();
	controller.run();

}

/** This function takes lower and upper ranges of indices
 * and runs test programs with all dimensions of these indices.
 * These programs test that the proper number of iterations has
 * been executed.  Jobs are pardo_loop_1d, ..., pardo_loop_6d.
 *
 * A variety of tests can be generated by changing the input params.
 */

void basic_pardo_test(int max_dims, int lower[], int upper[],
		bool expect_success = true) {
	assert(max_dims <= 6);
	for (int num_dims = 1; num_dims <= 6; ++num_dims) {
		std::stringstream job_ss;
		job_ss << "pardo_loop_" << num_dims << "d";
		std::string job = job_ss.str();

		//total number of iters for this sial program
		int num_iters = 1;
		for (int j = 0; j < num_dims; ++j) {
			num_iters *= ((upper[j] - lower[j]) + 1);
		}

		//create .dat file
		if (attr->global_rank() == 0) {
			init_setup(job.c_str());
			//add values for upper and lower bounds
			for (int i = 0; i < num_dims; ++i) {
				std::stringstream lower_ss, upper_ss;
				lower_ss << "lower" << i;
				upper_ss << "upper" << i;
				set_constant(lower_ss.str().c_str(), lower[i]);
				set_constant(upper_ss.str().c_str(), upper[i]);
			}
			std::string tmp = job + ".siox";
			const char* nm = tmp.c_str();
			add_sial_program(nm);
			finalize_setup();
		}

		TestControllerParallel controller(job, true, VERBOSE_TEST,
				"This is a test of " + job, std::cout, expect_success);
		controller.initSipTables();
		controller.run();
		if (attr->global_rank() == 0) {
			double total = controller.worker_->scalar_value("total");
			if (VERBOSE_TEST) {
				std::cout << "num_iters=" << num_iters << ", total=" << total
						<< std::endl;
			}
			EXPECT_EQ(num_iters, int(total));
		}
	}
}

TEST(Sial,pardo_loop) {
	int MAX_DIMS = 6;
	int lower[] = { 3, 2, 4, 1, 99, -1 };
	int upper[] = { 7, 6, 5, 1, 101, 2 };
	basic_pardo_test(6, lower, upper);
}

TEST(Sial,pardo_loop_corner_case) {
	int MAX_DIMS = 6;
	int lower[] = { 1, 1, 1, 1, 1, 1 };
	int upper[] = { 1, 1, 1, 1, 1, 1 };
	basic_pardo_test(6, lower, upper);
}

///*This case should fail with a message "FATAL ERROR: Pardo loop index i5 has empty range at :26"
// * IN addition to the assert throw, the controller constructor, which is in basic_pardo_test needs
// * to be passed false it final parameter, This param has default true, so is omitted in  most tests.
// */
//TEST(Sial,pardo_loop_illegal_range) {
//	int MAX_DIMS = 6;
//	int lower[] = { 1, 1, 1, 1, 1, 2 };
//	int upper[] = { 1, 1, 1, 1, 1, 1 };
//	ASSERT_THROW(basic_pardo_test(6, lower, upper, false), std::logic_error);
//
//}


TEST(Sial,put_test) {
	std::string job("put_test");
	int norb = 3;
	int segs[] = { 2, 3, 2 };
	if (attr->global_rank() == 0) {
		init_setup(job.c_str());
		set_constant("norb", norb);
		set_constant("norb_squared", norb * norb);
		std::string tmp = job + ".siox";
		const char* nm = tmp.c_str();
		add_sial_program(nm);
		set_aoindex_info(3, segs);
		finalize_setup();
	}
	std::stringstream output;

	TestControllerParallel controller(job, true, VERBOSE_TEST, "", output);
	controller.initSipTables();
	controller.run();
	if (attr->is_worker()) {
		EXPECT_TRUE(controller.worker_->all_stacks_empty());
		std::vector<int> index_vec;
		for (int i = 0; i < norb; ++i) {
			for (int j = 0; j < norb; ++j) {
				int k = (i * norb + j) + 1;
				index_vec.push_back(k);
				double * local_block = controller.local_block("result",
						index_vec);
				double value = local_block[0];
				double expected = k * k * segs[i] * segs[j];
				std::cout << "k,value= " << k << " " << value << std::endl;
				ASSERT_DOUBLE_EQ(expected, value);
				index_vec.clear();
			}
		}
	}

}

//TODO  restore functionality for single node version.  Was lost when PersistentArrayManager.h was refactored into
//worker and server versions.

TEST(Sial,persistent_scalars) {
	std::string job("persistent_scalars");
	double x = 3.456;
	double y = -0.1;

	{
		init_setup(job.c_str());
		set_scalar("x", x);
		set_scalar("y", y);
		std::string tmp1 = job + "_1.siox";
		const char* nm1 = tmp1.c_str();
		add_sial_program(nm1);
		std::string tmp2 = job + "_2.siox";
		const char* nm2 = tmp2.c_str();
		add_sial_program(nm2);
		finalize_setup();
	}

	std::stringstream output;
	TestControllerParallel controller(job, true, VERBOSE_TEST, "", output);
	controller.initSipTables();
	controller.run();
	if (attr->is_worker()) {
		ASSERT_DOUBLE_EQ(y, scalar_value("y"));
		ASSERT_DOUBLE_EQ(x, scalar_value("z"));
		ASSERT_DOUBLE_EQ(99.99, scalar_value("zz"));

		std::cout << "wpam:" << std::endl << *controller.wpam_ << std::endl
				<< "%%%%%%%%%%%%" << std::endl;
	}

	//Now do the second program
	//get siox name from setup, load and print the sip tables
	controller.initSipTables();
	controller.run();
	if (attr->is_worker()) {
		ASSERT_DOUBLE_EQ(x + 1, scalar_value("x"));
		ASSERT_DOUBLE_EQ(y, scalar_value("y"));
		ASSERT_DOUBLE_EQ(6, scalar_value("e"));
	}
}

TEST(Sial,get_mpi){
	std::string job("get_mpi");
	//create setup_file
	double x = 3.456;
	int norb = 4;
	int segs[]  = {2,3,4,1};

	if (attr->global_rank() == 0){
		init_setup(job.c_str());
		set_scalar("x",x);
		set_constant("norb",norb);
		std::string tmp = job + ".siox";
		const char* nm= tmp.c_str();
		add_sial_program(nm);
		set_aoindex_info(4,segs);
		finalize_setup();
	}

	std::stringstream output;
	TestControllerParallel controller(job, true, VERBOSE_TEST, "", output);
	controller.initSipTables();
	controller.run();

	if(attr->global_rank()==0){
		// Test a(1,1)
		// Get the data for local array block "b"
		int a_slot = controller.worker_->array_slot(std::string("a"));

		sip::index_selector_t a_indices_1;
		a_indices_1[0] = 1; a_indices_1[1] = 1;
		for (int i = 2; i < MAX_RANK; i++) a_indices_1[i] = sip::unused_index_value;
		sip::BlockId a_bid_1(a_slot, a_indices_1);
		std::cout << a_bid_1 << std::endl;
		sip::Block::BlockPtr a_bptr_1 = controller.worker_->get_block_for_reading(a_bid_1);
		sip::Block::dataPtr a_data_1 = a_bptr_1->get_data();
		std::cout << " Comparing block " << a_bid_1 << std::endl;
		for (int i=0; i<segs[0]; i++){
			for (int j=0; j<segs[0]; j++){
				ASSERT_DOUBLE_EQ(42*3, a_data_1[i*segs[0] + j]);
			}
		}
	}
}


//TEST(Sial,unmatched_get){
//	std::string job("unmatched_get");
//
//	double x = 3.456;
//	int norb = 4;
//
//	if (attr->global_rank() == 0){
//		init_setup(job.c_str());
//		set_scalar("x",x);
//		set_constant("norb",norb);
//		std::string tmp = job + ".siox";
//		const char* nm= tmp.c_str();
//		add_sial_program(nm);
//		int segs[]  = {2,3,4,1};
//		set_aoindex_info(4,segs);
//		finalize_setup();
//	}
//	std::stringstream output;
//	TestControllerParallel controller(job, true, VERBOSE_TEST, "This test should print a warning about unmatched get", output);
//	controller.initSipTables();
//	controller.run();
//}


TEST(Sial,delete_mpi){
	std::string job("delete_mpi");
	double x = 3.456;
	int norb = 4;

	if (attr->global_rank() == 0){
		init_setup(job.c_str());
		set_scalar("x",x);
		set_constant("norb",norb);
		std::string tmp = job + ".siox";
		const char* nm= tmp.c_str();
		add_sial_program(nm);
		int segs[]  = {2,3,4,1};
		set_aoindex_info(4,segs);
		finalize_setup();
	}

	std::stringstream output;
	TestControllerParallel controller(job, true, VERBOSE_TEST, "", output);
	controller.initSipTables();
	controller.run();
}


TEST(Sial,put_accumulate_mpi){
	std::string job("put_accumulate_mpi");
	double x = 3.456;
	int norb = 4;
	if (attr->global_rank() == 0){
		init_setup(job.c_str());
		set_scalar("x",x);
		set_constant("norb",norb);
		std::string tmp = job + ".siox";
		const char* nm= tmp.c_str();
		add_sial_program(nm);
		int segs[]  = {2,3,4,1};
		set_aoindex_info(4,segs);
		finalize_setup();
	}
	std::stringstream output;
	TestControllerParallel controller(job, true, VERBOSE_TEST, "", output);
	controller.initSipTables();
	controller.run();

}


/* TODO  check what this is testing.  It isn't clear that id doesn anyting */

TEST(Sial,all_rank_print){
	std::string job("all_rank_print_test");
	std::cout << "JOBNAME = " << job << std::endl;
	double x = 3.456;
	int norb = 2;
	if (attr->global_rank() == 0){
		init_setup(job.c_str());
		set_scalar("x",x);
		set_constant("norb",norb);
		std::string tmp = job + ".siox";
		const char* nm= tmp.c_str();
		add_sial_program(nm);
		int segs[]  = {2,3};
		set_aoindex_info(2,segs);
		finalize_setup();
	}

	std::stringstream output;
	TestControllerParallel controller(job, true, VERBOSE_TEST, "", output);
	controller.initSipTables();
	controller.run();
}


/* TODO check what this test does.  */
TEST(Sip,message_number_wraparound){

	std::string job("message_number_wraparound_test");

	if (attr->global_rank() == 0){
		init_setup(job.c_str());
		set_constant("norb",1);
		std::string tmp = job + ".siox";
		const char* nm= tmp.c_str();
		add_sial_program(nm);
		int segs[]  = {1};
		set_aoindex_info(1,segs);
		finalize_setup();
	}

	std::stringstream output;
	TestControllerParallel controller(job, true, VERBOSE_TEST, "", output);
	controller.initSipTables();
	controller.run();
}


TEST(Sial,persistent_distributed_array_mpi){
	std::string job("persistent_distributed_array_mpi");
	double x = 3.456;
	int norb = 2;
	int segs[]  = {2,3};

	if (attr->global_rank() == 0){
		init_setup(job.c_str());
		set_scalar("x",x);
		set_constant("norb",norb);
		std::string tmp = job + "1.siox";
		const char* nm= tmp.c_str();
		add_sial_program(nm);
		std::string tmp1 = job + "2.siox";
		const char* nm1= tmp1.c_str();
		add_sial_program(nm1);
		set_aoindex_info(2,segs);
		finalize_setup();
	}


	std::stringstream output;
	TestControllerParallel controller(job, true, true, "", output);

	//run first program
	controller.initSipTables();
	controller.run();

	std::cout << "Rank " << attr->global_rank() << " in persistent_distributed_array_mpi starting second program" << std::endl << std::flush;

	//run second program
	controller.initSipTables();
	controller.run();
	if (attr->is_worker()) {
		int i,j;
		for (i=1; i <= norb ; ++i ){
			for (j = 1; j <= norb; ++j){
			    double firstval = (i-1)*norb + j;
			    std::vector<int> indices;
			    indices.push_back(i);
			    indices.push_back(j);
			    double * block_data = controller.local_block(std::string("a"),indices);
			    size_t block_size = segs[i-1] * segs[j-1];
			    for (size_t count = 0; count < block_size; ++count){
			    	ASSERT_DOUBLE_EQ(3*firstval, block_data[count]);
			    	firstval++;
			    }
			}
		}
//||||||| merged common ancestors
//	//get siox name from setup, load and print the sip tables
//	std::string prog_name = setup_reader.sial_prog_list_.at(0);
//	std::string siox_dir(dir_name);
//	setup::BinaryInputFile siox_file(siox_dir + prog_name);
//	sip::SipTables sipTables(setup_reader, siox_file);
//
//	//create worker and server
//	if (sip_mpi_attr.global_rank()==0){   std::cout << "\n\n\n\nstarting SIAL PROGRAM  "<< job << std::endl;}
//
//
//	sip::DataDistribution data_distribution(sipTables, sip_mpi_attr);
//	sip::GlobalState::set_program_name(prog_name);
//	sip::GlobalState::increment_program();
//	if (sip_mpi_attr.is_server()){
//		sip::SIPServer server(sipTables, data_distribution, sip_mpi_attr, NULL);
//		MPI_Barrier(MPI_COMM_WORLD);
//		std::cout<<"starting server" << std::endl;
//		server.run();
//		std::cout << "Server state after termination" << server << std::endl;
//	} else {
//		sip::SialxTimer sialxTimer(sipTables.max_timer_slots());
//		sip::Interpreter runner(sipTables, sialxTimer,  NULL);
//		MPI_Barrier(MPI_COMM_WORLD);
//		std::cout << "starting worker for "<< job  << std::endl;
//		runner.interpret();
//		std::cout << "\nSIAL PROGRAM TERMINATED"<< std::endl;
//=======
//	//get siox name from setup, load and print the sip tables
//	std::string prog_name = setup_reader.sial_prog_list().at(0);
//	std::string siox_dir(dir_name);
//	setup::BinaryInputFile siox_file(siox_dir + prog_name);
//	sip::SipTables sipTables(setup_reader, siox_file);
//
//	//create worker and server
//	if (sip_mpi_attr.global_rank()==0){   std::cout << "\n\n\n\nstarting SIAL PROGRAM  "<< job << std::endl;}
//
//
//	sip::DataDistribution data_distribution(sipTables, sip_mpi_attr);
//	sip::GlobalState::set_program_name(prog_name);
//	sip::GlobalState::increment_program();
//	if (sip_mpi_attr.is_server()){
//		sip::SIPServer server(sipTables, data_distribution, sip_mpi_attr, NULL);
//		MPI_Barrier(MPI_COMM_WORLD);
//		std::cout<<"starting server" << std::endl;
//		server.run();
//		std::cout << "Server state after termination" << server << std::endl;
//	} else {
//		sip::SialxTimer sialxTimer(sipTables.max_timer_slots());
//		sip::Interpreter runner(sipTables, sialxTimer,  NULL);
//		MPI_Barrier(MPI_COMM_WORLD);
//		std::cout << "starting worker for "<< job  << std::endl;
//		runner.interpret();
//		std::cout << "\nSIAL PROGRAM TERMINATED"<< std::endl;
//>>>>>>> origin/refactor_opcodes
	}
}




//****************************************************************************************************************

void bt_sighandler(int signum) {
	std::cerr << "Interrupt signal (" << signum << ") received." << std::endl;
	FAIL();
	abort();
}

int main(int argc, char **argv) {

//    feenableexcept(FE_DIVBYZERO);
//    feenableexcept(FE_OVERFLOW);
//    feenableexcept(FE_INVALID);
//
//    signal(SIGSEGV, bt_sighandler);
//    signal(SIGFPE, bt_sighandler);
//    signal(SIGTERM, bt_sighandler);
//    signal(SIGINT, bt_sighandler);
//    signal(SIGABRT, bt_sighandler);

#ifdef HAVE_MPI
	MPI_Init(&argc, &argv);
	int num_procs;
	sip::SIPMPIUtils::check_err(MPI_Comm_size(MPI_COMM_WORLD, &num_procs), __LINE__,__FILE__);

	if (num_procs < 2) {
		std::cerr << "Please run this test with at least 2 mpi ranks"
				<< std::endl;
		return -1;
	}
	sip::SIPMPIUtils::set_error_handler();
	sip::SIPMPIAttr &sip_mpi_attr = sip::SIPMPIAttr::get_instance();
	attr = &sip_mpi_attr;
#endif
	barrier();
#ifdef HAVE_TAU
	TAU_PROFILE_SET_NODE(0);
	TAU_STATIC_PHASE_START("SIP Main");
#endif

//	sip::check(sizeof(int) >= 4, "Size of integer should be 4 bytes or more");
//	sip::check(sizeof(double) >= 8, "Size of double should be 8 bytes or more");
//	sip::check(sizeof(long long) >= 8, "Size of long long should be 8 bytes or more");
//
//	int num_procs;
//	sip::SIPMPIUtils::check_err(MPI_Comm_size(MPI_COMM_WORLD, &num_procs));
//
//	if (num_procs < 2){
//		std::cerr<<"Please run this test with at least 2 mpi ranks"<<std::endl;
//		return -1;
//	}
//
//	sip::SIPMPIUtils::set_error_handler();
//	sip::SIPMPIAttr &sip_mpi_attr = sip::SIPMPIAttr::get_instance();
//

	printf("Running main() from test_sial.cpp\n");
	testing::InitGoogleTest(&argc, argv);
	barrier();
	int result = RUN_ALL_TESTS();

	std::cout << "Rank  " << attr->global_rank() << " Finished RUN_ALL_TEST() " << std::endl << std::flush;

#ifdef HAVE_TAU
	TAU_STATIC_PHASE_STOP("SIP Main");
#endif
	barrier();
#ifdef HAVE_MPI
	MPI_Finalize();
#endif
	return result;

}
