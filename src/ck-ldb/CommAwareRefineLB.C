/** \file CommAwareRefineLB.C
 *
 *  Written by Harshitha Menon
 *  
 *  This Loadbalancer strategy is Refine but taking into consideration the
 *  Communication between the processors.
 *  The following are the steps in the loadbalancing strategy
 *
 *  1. Construct a max heap of processor load whose load is greater than avg
 *  2. Construct a sorted array of processor load whose load is less than avg
 *  3. Pick the heaviest processor from the heap, randomly select a chare in
 *  that processor and decide on which processor in the underloaded processor
 *  list to transfer it to based on the one with which it is 
 *  heavily communicating.
 *  4. If the load of the processors after the transfer is less than the avg
 *  load, then push it into the underloaded processor list, else push it into
 *  the max heap.
 */

/**
 * \addtogroup CkLdb
*/
/*@{*/

#include "CommAwareRefineLB.h"
#include "ckgraph.h"
#include <algorithm>
#include <map>

#include <time.h>

#define THRESHOLD 0.1
#define SWAP_MULTIPLIER 5 

inline void eraseObjFromParrObjs(std::vector<int> & parr_objs, int remove_objid);
inline void printMapping(std::vector<Vertex> &vertices);
inline void removeFromArray(int pe_id, std::vector<int> &array);
inline int popFromProcHeap(std::vector<int> & parr_above_avg, ProcArray *parr);
inline void handleTransfer(int randomly_obj_id, ProcInfo& p, int possible_pe, std::vector<int> *parr_objs, ObjGraph *ogr, ProcArray* parr);
inline void updateLoadInfo(int p_index, int possible_pe, double upper_threshold, double lower_threshold,
                           std::vector<int> &parr_above_avg, std::vector<int> &parr_below_avg,
                           bool* proc_load_info, ProcArray *parr);

double upper_threshold;
double lower_threshold;

CreateLBFunc_Def(CommAwareRefineLB, "always assign the heaviest obj onto lightest loaded processor.")

CommAwareRefineLB::CommAwareRefineLB(const CkLBOptions &opt): CentralLB(opt)
{
  lbname = "CommAwareRefineLB";
  if (CkMyPe()==0)
    CkPrintf("[%d] CommAwareRefineLB created\n",CkMyPe());
}

CmiBool CommAwareRefineLB::QueryBalanceNow(int _step)
{
  //  CkPrintf("[%d] Balancing on step %d\n",CkMyPe(),_step);
  return CmiTrue;
}

class ProcLoadGreater {
  public:
    ProcLoadGreater(ProcArray *parr) : parr(parr) {
    }
    bool operator()(int lhs, int rhs) {
      return (parr->procs[lhs].getTotalLoad() < parr->procs[rhs].getTotalLoad());
    }

  private:
    ProcArray *parr;
};

class ObjLoadGreater {
  public:
    bool operator()(Vertex v1, Vertex v2) {
      return (v1.getVertexLoad() > v2.getVertexLoad());
    }
};

class PeCommInfo {
  public:
    PeCommInfo() : num_msg(0), num_bytes(0) {
    }

    PeCommInfo(int pe_id) : pe_id(pe_id), num_msg(0) , num_bytes(0) {
    }
    int pe_id;
    int num_msg;
    int num_bytes;
    // TODO: Should probably have a communication cost
};

// Consists of communication information of an object with is maintained
// as a list of PeCommInfo containing the processor id and the bytes transferred
class ObjPeCommInfo {
  public:
    ObjPeCommInfo() {
    }

    int obj_id;
    std::vector<PeCommInfo> pcomm;
};

class ProcCommGreater {
  public:
    bool operator()(PeCommInfo p1, PeCommInfo p2) {
      // TODO(Harshitha): Should probably consider total communication cost
      return (p1.num_bytes > p2.num_bytes);
    }
};

void PrintProcLoad(ProcArray *parr) {
  int vert;
  double pe_load;
  for (vert = 0; vert < parr->procs.size(); vert++) {
    pe_load = parr->procs[vert].getTotalLoad();
    if (pe_load > upper_threshold) {
      CkPrintf("Above load : %d load : %E overhead : %E\n",
        parr->procs[vert].getProcId(), parr->procs[vert].getTotalLoad(),
        parr->procs[vert].overhead());
    } else if (pe_load < lower_threshold) {
      CkPrintf("Below load : %d load : %E overhead : %E\n",
        parr->procs[vert].getProcId(), parr->procs[vert].getTotalLoad(),
        parr->procs[vert].overhead());
    } else {
      CkPrintf("Within avg load : %d load : %E overhead : %E\n",
        parr->procs[vert].getProcId(), parr->procs[vert].getTotalLoad(),
        parr->procs[vert].overhead());
    }
  }
}

void PrintProcObj(ProcArray *parr, std::vector<int>* parr_objs) {
  int i, j;
  CkPrintf("---------------------\n");
  for (i = 0; i < parr->procs.size(); i++) {
    CkPrintf("[%d] contains ", i);
    for (j = 0; j < parr_objs[i].size(); j++) {
      CkPrintf(" %d, ", parr_objs[i][j]);
    }
    CkPrintf("\n");
  }
  CkPrintf("---------------------\n");
}


void CommAwareRefineLB::work(LDStats* stats)
{
  /** ========================== INITIALIZATION ============================= */
  ProcArray *parr = new ProcArray(stats);       // Processor Array
  ObjGraph *ogr = new ObjGraph(stats);          // Object Graph
  int vertexid_pe[ogr->vertices.size()]; // Stores the cur pe of obj
  double avgload = parr->getAverageLoad();      // Average load of processors

  // Sets to false if it is overloaded, else to true
  bool* proc_load_info = new bool[parr->procs.size()];
  memset(proc_load_info, false, parr->procs.size());

  // Create an array of vectors for each processor mapping to the objects in
  // that processor
  std::vector<int>* parr_objs = new std::vector<int>[parr->procs.size()];

  // Create an array for each chare with processor communication info
  ObjPeCommInfo* objpcomm = new ObjPeCommInfo[ogr->vertices.size()];

  upper_threshold = avgload + (avgload * THRESHOLD);
  lower_threshold = avgload - (avgload * THRESHOLD * THRESHOLD);

  int less_loaded_counter = 0;

  srand(time(NULL));
  /** ============================= STRATEGY ================================ */

  if (_lb_args.debug()>1) 
    CkPrintf("[%d] In CommAwareRefineLB strategy\n",CkMyPe());

  CkPrintf("Average load %E\n", avgload);

  int vert, i, j;
  int curr_pe;

  // Iterate over all the chares and construct the peid, vector<chareid> array
  for(vert = 0; vert < ogr->vertices.size(); vert++) {
    curr_pe = ogr->vertices[vert].getCurrentPe();
    parr_objs[curr_pe].push_back(vert);
    vertexid_pe[ogr->vertices[vert].getVertexId()] = curr_pe;
  }


  std::vector<int> parr_above_avg;
  std::vector<int> parr_below_avg;

  double pe_load;

  // Insert into parr_above_avg if the processor fits under the criteria of
  // overloaded processor.
  // Insert the processor id into parr_below_avg if the processor is underloaded 
  for (vert = 0; vert < parr->procs.size(); vert++) {
    pe_load = parr->procs[vert].getTotalLoad();
    if (pe_load > upper_threshold) {
      // Pushing ProcInfo into this list
      parr_above_avg.push_back(vert);
    } else if (pe_load < lower_threshold) {
      parr_below_avg.push_back(parr->procs[vert].getProcId());
      proc_load_info[parr->procs[vert].getProcId()] = true;
      less_loaded_counter++;
    }
  }

  std::make_heap(parr_above_avg.begin(), parr_above_avg.end(),
      ProcLoadGreater(parr));

  // Construct the datastructure that stores communication information for each
  // chare with respect to processors
  for (vert = 0; vert < ogr->vertices.size(); vert++) {
    std::map<int, int> tmp_map_pid_index;
    int counter = objpcomm[vert].pcomm.size();
    int index;
    for (i = 0; i < ogr->vertices[vert].sendToList.size(); i++) {
      j = vertexid_pe[ogr->vertices[vert].sendToList[i].getNeighborId()];
      // TODO: Should it index with vertexId?
      if (tmp_map_pid_index.count(j) == 0) {
        tmp_map_pid_index[j] = counter;
        PeCommInfo pecomminf(j);
        // TODO: Shouldn't it use vertexId instead of vert?
        objpcomm[vert].pcomm.push_back(pecomminf);
        counter++;
      }
      index = tmp_map_pid_index[j];
    
      objpcomm[vert].pcomm[index].num_msg +=
        ogr->vertices[vert].sendToList[i].getNumMsgs();
      objpcomm[vert].pcomm[index].num_bytes +=
        ogr->vertices[vert].sendToList[i].getNumBytes();
    }

    for (i = 0; i < ogr->vertices[vert].recvFromList.size(); i++) {
      j = vertexid_pe[ogr->vertices[vert].recvFromList[i].getNeighborId()];

      if (tmp_map_pid_index.count(j) == 0) {
        tmp_map_pid_index[j] = counter;
        PeCommInfo pecomminf(j);
        // TODO: Shouldn't it use vertexId instead of vert?
        objpcomm[vert].pcomm.push_back(pecomminf);
        counter++;
      }
      index = tmp_map_pid_index[j];

      objpcomm[vert].pcomm[index].num_msg +=
        ogr->vertices[vert].sendToList[i].getNumMsgs();
      objpcomm[vert].pcomm[index].num_bytes +=
        ogr->vertices[vert].sendToList[i].getNumBytes();
    }

    // Sort the pe communication vector for this chare
    std::sort(objpcomm[vert].pcomm.begin(), objpcomm[vert].pcomm.end(),
        ProcCommGreater());
  }

  int random;
  int randomly_obj_id;
  bool obj_allocated;
  int num_tries;
  // Allow as many swaps as there are chares
  int total_swaps = ogr->vertices.size() * SWAP_MULTIPLIER;
  int possible_pe;
  double obj_load;

  // Keep on loadbalancing until the number of above avg processors is 0
  while (parr_above_avg.size() != 0 && total_swaps > 0 && parr_below_avg.size() != 0) {
      // CkPrintf("Above avg : %d Below avg : %d Total swaps: %d\n", parr_above_avg.size(),
      //    parr_below_avg.size(), total_swaps);
      obj_allocated = false;
      num_tries = 0;

      // Pop the heaviest processor
      int p_index = popFromProcHeap(parr_above_avg, parr);
      ProcInfo& p = parr->procs[p_index];

      while (!obj_allocated && num_tries < 5) {

        // It might so happen that due to overhead load, it might not have any
        // more objects in its list
        if (parr_objs[p.getProcId()].size() == 0) {
          // CkPrintf("No obj left to be allocated\n");
          obj_allocated = true;
          break;
        }

        int randd = rand();
        random = randd % parr_objs[p.getProcId()].size();
        randomly_obj_id = parr_objs[p.getProcId()][random];
        obj_load = ogr->vertices[randomly_obj_id].getVertexLoad();

        // CkPrintf("Heavy %d: Parr obj size : %d random : %d random obj id : %d\n", p_index,
        //     parr_objs[p.getProcId()].size(), randd, randomly_obj_id);
        for (i = 0; i < objpcomm[randomly_obj_id].pcomm.size(); i++) {

          // If the heaviest communicating processor is there in the list, then
          // assign it to that.
          possible_pe = objpcomm[randomly_obj_id].pcomm[i].pe_id;

          if ((parr->procs[possible_pe].getTotalLoad() + obj_load) < upper_threshold) {
            //CkPrintf("** Iter[%d] : Transfered %d from %d : Load %E to %d : Load %E Comm : %d\n",
            //    i, randomly_obj_id, p.getProcId(), p.getTotalLoad(),
            //    possible_pe, parr->procs[possible_pe].getTotalLoad(),
            //    objpcomm[randomly_obj_id].pcomm[i].num_msg);

            handleTransfer(randomly_obj_id, p, possible_pe, parr_objs, ogr, parr);
            obj_allocated = true;
            total_swaps--;
            updateLoadInfo(p_index, possible_pe, upper_threshold, lower_threshold,
                parr_above_avg, parr_below_avg, proc_load_info, parr);

            break;
          }
        }

        // Since there is no processor in the least loaded list with which this
        // chare communicates, pick a random least loaded processor.
        if (!obj_allocated) {
          // CkPrintf(":( Could not transfer to the nearest communicating ones\n");
          for (int x = 0; x < parr_below_avg.size(); x++) {
            int random_pe = parr_below_avg[x];
            if ((parr->procs[random_pe].getTotalLoad() + obj_load) < upper_threshold) {
              obj_allocated = true;
              total_swaps--;
              handleTransfer(randomly_obj_id, p, random_pe, parr_objs, ogr, parr);
              updateLoadInfo(p_index, random_pe, upper_threshold, lower_threshold,
                  parr_above_avg, parr_below_avg, proc_load_info, parr);
              break;
            }
            num_tries++;
          }
        }
      }

      if (!obj_allocated) {
        CkPrintf("!!!! Could not handle the heavy proc %d so giving up\n", p_index);
       // parr_above_avg.push_back(p_index);
       // std::push_heap(parr_above_avg.begin(), parr_above_avg.end(),
       //     ProcLoadGreater(parr));
      }
    }


  /** ============================== CLEANUP ================================ */
  ogr->convertDecisions(stats);         // Send decisions back to LDStats
  delete parr;
  delete ogr;
  delete[] parr_objs;
  delete[] objpcomm;
}

inline void eraseObjFromParrObjs(std::vector<int> & parr_objs, int remove_objid) {
  for (int i = 0; i < parr_objs.size(); i++) {
    if (parr_objs[i] == remove_objid) {
      parr_objs.erase(parr_objs.begin() + i);
      return;
    }
  }
}


inline void printMapping(std::vector<Vertex> &vertices) {
  for (int i = 0; i < vertices.size(); i++) {
    CkPrintf("%d: old map : %d new map : %d\n", i, vertices[i].getCurrentPe(),
        vertices[i].getNewPe());
  }
}

inline void removeFromArray(int pe_id, std::vector<int> &array) {
  for (int i = 0; i < array.size(); i++) {
    if (array[i] == pe_id) {
      array.erase(array.begin() + i);
    }
  }
}

inline int popFromProcHeap(std::vector<int> & parr_above_avg, ProcArray *parr) {
  int p_index = parr_above_avg.front();
  std::pop_heap(parr_above_avg.begin(), parr_above_avg.end(),
      ProcLoadGreater(parr));
  parr_above_avg.pop_back();
  return p_index;
}

    
inline void handleTransfer(int randomly_obj_id, ProcInfo& p, int possible_pe, std::vector<int>* parr_objs, ObjGraph *ogr, ProcArray* parr) {
  ogr->vertices[randomly_obj_id].setNewPe(possible_pe);
  parr_objs[possible_pe].push_back(randomly_obj_id);
  ProcInfo &possible_pe_procinfo = parr->procs[possible_pe];

  p.totalLoad() -= ogr->vertices[randomly_obj_id].getVertexLoad();
  possible_pe_procinfo.totalLoad() += ogr->vertices[randomly_obj_id].getVertexLoad();
  eraseObjFromParrObjs(parr_objs[p.getProcId()], randomly_obj_id);
  //CkPrintf("After transfered %d from %d : Load %E to %d : Load %E\n", randomly_obj_id, p.getProcId(), p.getTotalLoad(),
  //    possible_pe, possible_pe_procinfo.getTotalLoad());
}

inline void updateLoadInfo(int p_index, int possible_pe, double upper_threshold, double lower_threshold,
                           std::vector<int>& parr_above_avg, std::vector<int>& parr_below_avg,
                           bool* proc_load_info, ProcArray *parr) {

  ProcInfo& p = parr->procs[p_index];
  ProcInfo& possible_pe_procinfo = parr->procs[possible_pe];

  // If the updated load is still greater than the average by the
  // threshold value, then push it back to the max heap
  if (p.getTotalLoad() > upper_threshold) {
    parr_above_avg.push_back(p_index);
    std::push_heap(parr_above_avg.begin(), parr_above_avg.end(),
        ProcLoadGreater(parr));
    //CkPrintf("\t Pushing pe : %d to max heap\n", p.getProcId());
  } else if (p.getTotalLoad() < lower_threshold) {
    parr_below_avg.push_back(p_index);
    proc_load_info[p_index] = true;
    //CkPrintf("\t Adding pe : %d to less loaded\n", p.getProcId());
  }

  // If the newly assigned processor's load is greater than the average
  // by the threshold value, then push it into the max heap.
  if (possible_pe_procinfo.getTotalLoad() > upper_threshold) {
    // TODO: It should be the index in procarray :(
    parr_above_avg.push_back(possible_pe);
    std::push_heap(parr_above_avg.begin(), parr_above_avg.end(),
        ProcLoadGreater(parr));
    removeFromArray(possible_pe, parr_below_avg);
    proc_load_info[possible_pe] = false;
    //CkPrintf("\t Pusing pe : %d to max heap\n", possible_pe);
  } else if (possible_pe_procinfo.getTotalLoad() < lower_threshold) {
  } else {
    removeFromArray(possible_pe, parr_below_avg);
    proc_load_info[possible_pe] = false;
    //CkPrintf("\t Removing from lower list pe : %d\n", possible_pe);
  }

}

#include "CommAwareRefineLB.def.h"

/*@}*/
