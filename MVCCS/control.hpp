
#ifndef __CONTROL_HPP__
#define __CONTROL_HPP__

#include "rowstate.hpp"
#include "rowlock.hpp"
#include <ctime>

namespace Zigurat
{

  class Control
  {
  public:
    // hexmap part
    RowState online_state = RowState::NONE;
    RowLock online_lock = RowLock::NONE;

    RowState offline_state = RowState::NONE;
    RowLock offline_lock = RowLock::NONE;

    // data part
    time_t online_time = (time_t)0;
    size_t transaction_id = 0;
    
    int64_t query_id = 0;
    int64_t reference_address = -1;

    time_t modify_time = (time_t)0;
    time_t create_time = (time_t)0;
  };
  
}

#endif // __CONTROL_HPP__
