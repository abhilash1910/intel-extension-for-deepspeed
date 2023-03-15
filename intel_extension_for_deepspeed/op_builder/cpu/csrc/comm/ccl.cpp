#include <torch/extension.h>

#include <oneapi/ccl.hpp>
//#include <mpi.h>
#if 0
#include <chrono>
#include <pybind11/embed.h>
namespace py = pybind11;

#include <c10/util/irange.h>

#include <iostream>
#include <string>

#include <comm.h>

// TODO: remove
#include <stdio.h>

#endif

std::set<int> _comm_ids;
std::set<int> _colors;
//std::unordered_map<int, ccl::communicator> _ccl_comms;
ccl::vector_class<ccl::communicator> _ccl_comms;
void create_comm_group(std::vector<int> comm_ranks, int rank, int comm_id, int color);

#define MPICHECK(cmd)                                                        \
    do {                                                                     \
        int e = cmd;                                                         \
        if (e != MPI_SUCCESS) {                                              \
            printf("Failed: MPI error %s:%d '%d'\n", __FILE__, __LINE__, e); \
            exit(EXIT_FAILURE);                                              \
        }                                                                    \
    } while (0)

#define CCLCHECK(cmd)                                                                           \
    do {                                                                                         \
        cmd;                                                                  \
    } while (0)

#define KVS_CREATE_SUCCESS 0
#define KVS_CREATE_FAILURE -1

bool is_initialized = 0;

bool use_mpi = 0;

int world_rank = -1;
int world_size = -1;

void initialize(int rank, int size)
{
    if (is_initialized) return;
    world_size = size;
    world_rank = rank;
    is_initialized = 1;
    _ccl_comms.emplace_back(ccl::preview::create_communicator());
}

int get_rank(int group = 0)
{
    return world_rank;
}

int get_world_size(int group = 0)
{
    return world_size;
}

// Find the next ordered, unique value to a set. E.g. <0,1,2,7> --> 3
int next_unique_val(std::set<int> s) {
    std::set<int>::iterator itr;
    // Base case. Add 0 to start of set.
    if (s.empty() || *s.begin() != 0) {
        return 0;
    // second base case where s = {0} (the case of s = {n != 0} is caught above)
    } else if (s.size() == 1) {
        return 1;
    } else {
        int prev_val = *s.begin();
        for (itr = std::next(s.begin()); itr != s.end(); itr++) {
            if (*itr != prev_val + 1) {
                return prev_val + 1;
            }
            prev_val = *itr;
        }
        return *(s.end()) + 1;
    }
}

py::object new_group(std::vector<int> ranks) {
    //std::cout << "RANK: " << get_rank() << " COMM_ID: " << comm_id << " COLOR: " << color << std::endl;
    int comm_id = next_unique_val(_comm_ids);
    int color = next_unique_val(_colors);
    create_comm_group(ranks, get_rank(), comm_id, color);
    py::object ProcessGroup = py::module_::import("deepspeed.comm").attr("ProcessGroup");
    py::object newPG = ProcessGroup(comm_id, ranks);
    return newPG;
}

void create_comm_group(std::vector<int> comm_ranks, int rank, int comm_id, int color)
{
    return;
    #if 0
    int world_rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    ccl::cclComm_t _ccl_comm;
    MPI_Comm _comm;
    MPI_Comm_dup(MPI_COMM_WORLD, &_comm);
    MPI_Comm_group(_comm, &_group);
    unsigned num_ranks = comm_ranks.size();
    MPI_Comm _newcomm;
    if (num_ranks < world_size) {
        auto total_group = _group;
        MPI_Group_incl(total_group, num_ranks, comm_ranks.data(), &_group);
        MPI_Comm_split(_comm, color, 0, &_newcomm);
        int local_world_rank, local_world_size;
        MPI_Comm_rank(_newcomm, &local_world_rank);
        MPI_Comm_size(_newcomm, &local_world_size);
    } else if (num_ranks > world_size) {
        auto message = std::string(
            "Fail to create comm group (number of ranks is higher than world_size).");
        std::cerr << message << std::endl;
        throw std::runtime_error(message);
    }
    cclUniqueId _ccl_uid;
    if (rank == comm_ranks[0]) {
        cclGetUniqueId(&_ccl_uid);
    }
    MPI_Bcast((void*)&_ccl_uid,
              sizeof(cclUniqueId),
              MPI_BYTE,
              comm_ranks[0],
              num_ranks < world_size ? _newcomm : _comm);
    if(std::find(comm_ranks.begin(), comm_ranks.end(), rank) != comm_ranks.end()) {
        cclCommInitRank(&_ccl_comm, num_ranks, _ccl_uid, rank % num_ranks);
    }
    _comm_created = true;
    _world_sizes[comm_id] = num_ranks;
    _ccl_comms[comm_id] = _ccl_comm;
    _color_map[comm_id] = color;
    _comm_ids.insert(comm_id);
    _colors.insert(color);
    #endif
}


ccl::datatype get_ccl_datatype(c10::ScalarType type)
{
    ccl::datatype ccl_type;
    switch (type) {
        case c10::ScalarType::Int: ccl_type = ccl::datatype::int32; break;
        case c10::ScalarType::Float: ccl_type = ccl::datatype::float32; break;
        case c10::ScalarType::Double: ccl_type = ccl::datatype::float64; break;
        case c10::ScalarType::BFloat16: ccl_type = ccl::datatype::bfloat16; break;
        case c10::ScalarType::Half: ccl_type = ccl::datatype::float16; break;
        default: ccl_type = ccl::datatype::int8;
    }
    return ccl_type;
}

#if 0
cclComm_t _get_comm_from_group(py::object group);

cudaStream_t s;
cclComm_t _world_ccl_comm;

//py::module_ dist = py::module_::import("deepspeed.comm");

std::vector<cclComm_t> global_ccl_comms;
std::vector<cudaStream_t> global_streams;


//REZA+AMMAR CODE
//curandGenerator_t _gen;
//cublasHandle_t _cublasHandle; 
cudaEvent_t _comp_event;
cudaEvent_t _comm_event;    
void* _workspace;
uint64_t _seed;
uint64_t _curr_offset;
size_t _workSpaceSize;
unsigned _token_length;
unsigned _num_tokens;
std::vector<std::array<int, 3>> _gemm_algos;    
cudaStream_t _comp_stream;
cudaStream_t _comm_stream;  
MPI_Group _group;
std::unordered_map<int, int> _world_sizes;
std::unordered_map<int, int> _color_map;
MPI_Comm _comm;
bool _comm_created;
//py::object ProcessGroup = py::module_::import("deepspeed.comm").attr("ProcessGroup");
//py::object world_group;

int get_rank(int group = 0)
{
    int world_rank;
    MPICHECK(MPI_Comm_rank(MPI_COMM_WORLD, &world_rank));
    return world_rank;
}


// Given a cclUniqueId, convert it to a string representation that can be put
// in the store.
std::string buildCclUniqueIdStr(const cclUniqueId& cclID)
{
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&cclID);
    std::ostringstream oss;
    for (const auto i : c10::irange(CCL_UNIQUE_ID_BYTES)) {
        oss << std::hex << static_cast<int>(bytes[i]);
    }
    return oss.str();
}

std::string getCclId()
{
    cclUniqueId cclID;
    CCLCHECK(cclGetUniqueId(&cclID));
    return buildCclUniqueIdStr(cclID);

    // std::string id = "hello";
    // for (int i=0; i<128; i++)
    //     std::cout << "cclID =" << cclID[i];
    // std::cout<< std::endl;
    // return id;
}


/*
py::object get_world_group() {
    int world_size = get_world_size(0);
    std::vector<int> ranks(world_size);
    std::iota(ranks.begin(), ranks.end(), 0);
    py::object ProcessGroup = py::module_::import("deepspeed.comm").attr("ProcessGroup");
    return ProcessGroup(0, ranks);
}
*/

void _print_comm_number() { std::cout << "Number of Sub-Comms:" << _ccl_comms.size() + 1 << "\n"; }

cudaStream_t GetCommStream(bool async_op = false)
{
    if (!_comm_stream)
        _comm_stream = async_op ? at::cuda::getStreamFromPool(true)
                                : at::cuda::getCurrentCUDAStream();
    return _comm_stream;
}

void finalize()
{
    CCLCHECK(cclCommDestroy(_world_ccl_comm));
}




/*
void test_set() {
    std::set<int> val1 = {6, 5, 10, 1};
    std::set<int> val2 = {};
    std::set<int> val3 = {0};
    std::set<int> val4 = {0,1,2,3,6,4};
    if (get_rank() == 0) {
        std::cout << next_unique_val(val4) << std::endl;
    }
}
*/



#endif

ccl::reduction get_ccl_reduce_op(py::object op, at::Tensor& input)
{
    py::object ReduceOp = py::module_::import("deepspeed.comm").attr("ReduceOp");
    if (!py::isinstance(op, ReduceOp)) {
        throw std::runtime_error ("Error: Op must be of type ReduceOp");
    }

    int op_val = py::int_(op.attr("value"));
    ccl::reduction ccl_op;

    if (input.scalar_type() == at::kBool) {
        if (op_val == (int)py::int_(ReduceOp.attr("SUM").attr("value"))) {
            // For bool tensors, map sum to max, which both represent a bitwise or.
            // This is to prevent overflow issues with sum, since we use uint8 to
            // represent a bool (see cclDataType mapping).
            ccl_op = ccl::reduction::max;
        } else if (op_val == (int)py::int_(ReduceOp.attr("AVG").attr("value"))) {
            throw std::runtime_error ("Error: For bool tensors, op must be of type ReduceOp");
        }
    }

    if (op_val == (int)py::int_(ReduceOp.attr("SUM").attr("value"))) {
        ccl_op = ccl::reduction::sum;
    } else if (op_val == (int)py::int_(ReduceOp.attr("MIN").attr("value"))) {
        ccl_op = ccl::reduction::min;
    } else if (op_val == (int)py::int_(ReduceOp.attr("MAX").attr("value"))) {
        ccl_op = ccl::reduction::max;
    } else if (op_val == (int)py::int_(ReduceOp.attr("PRODUCT").attr("value"))) {
        ccl_op = ccl::reduction::prod;
    } else {
        throw std::runtime_error ("Error: Unrecognized ReduceOp type");
    }
    return ccl_op;
}

ccl::communicator& _get_comm_from_group() {
    return _ccl_comms[0];
}

ccl::communicator& _get_comm_from_group(py::object group) {
    return _ccl_comms[0];
    #if 0
    if (group == Py_None) {
        return _ccl_comms[0];
    } else {
        py::object ProcessGroup = py::module_::import("torch.distributed").attr("ProcessGroup");
        if (!py::isinstance(group, ProcessGroup)) {
            throw std::runtime_error ("Error: group must be of type ProcessGroup");
        }
        return _ccl_comms[py::int_(group.attr("comm_id"))];
    }
    #endif
}

void broadcast(torch::Tensor& data, int src, py::object group, bool async_op)
{
    CCLCHECK(ccl::broadcast(data.data_ptr(),
                            data.numel(),
                            get_ccl_datatype(data.scalar_type()),
                            src,
                            _get_comm_from_group(group)).wait());
}

//TODO: implement torch's async_op behavior, document it.
void all_reduce(torch::Tensor& data, py::object op, py::object group, bool async_op)
{
    CCLCHECK(ccl::allreduce(data.data_ptr(),
                            data.data_ptr(),
                            data.numel(),
                            get_ccl_datatype(data.scalar_type()),
                            get_ccl_reduce_op(op, data),
                            _get_comm_from_group(group)).wait());
}

void barrier(py::object group, bool async_op) {
    CCLCHECK(ccl::barrier(_get_comm_from_group(group)).wait());
}

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m)
{
    //m.def("getCclId", &getCclId, "Get Unique CCL ID");
    m.def("initialize", &initialize, "ccl initialize");
    //m.def("finalize", &finalize, "ccl finalize");
    m.def("all_reduce", &all_reduce, "ccl all_reduce");
    m.def("barrier", &barrier, "barrier");
    //m.def("send", &send, "ccl send");
    //m.def("recv", &recv, "ccl recv");
    m.def("broadcast", &broadcast, "ccl broadcast");
    //m.def("all_to_all_single", &all_to_all_single, "ccl alltoall");
    //m.def("all_toall_list", &all_to_all, "ccl alltoall list");
    //m.def("all_gather_base", &all_gather_base, "ccl all_gather_base");
    //m.def("all_gather", &all_gather, "ccl all_gather");
    //m.def("reduce", &reduce, "ccl reduce");
    //m.def("reduce_scatter", &reduce_scatter, "ccl reduce scatter");
    m.def("get_rank", &get_rank, "get rank");
    m.def("get_world_size", &get_world_size, "get world size");
    //m.def("create_comm_group", &create_comm_group, "manually create comm group");
    //m.def("test_set", &test_set, "manually create comm group");
    m.def("new_group", &new_group, "automatically create comm group");
    //m.def("get_world_group", &get_world_group, "Returns the WORLD process group");
}
