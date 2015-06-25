// Copyright (c) 2015 DisplayLink (UK) Ltd.

#ifndef EVDI_DEBUG_H
#define EVDI_DEBUG_H

#define EVDI_LOGLEVEL_ALWAYS  0
#define EVDI_LOGLEVEL_FATAL   1
#define EVDI_LOGLEVEL_ERROR   2
#define EVDI_LOGLEVEL_WARN    3
#define EVDI_LOGLEVEL_INFO    4
#define EVDI_LOGLEVEL_DEBUG   5
#define EVDI_LOGLEVEL_VERBOSE 6

extern unsigned g_evdi_loglevel;

#define EVDI_PRINTK(lEVEL, pREFIX, ...) \
  do { if (lEVEL <= g_evdi_loglevel) {\
    printk("[%s] %s ", pREFIX, __FUNCTION__); \
    printk(__VA_ARGS__); \
  } } while(false);

#define EVDI_LOG(...) EVDI_PRINTK(EVDI_LOGLEVEL_ALWAYS, " ", __VA_ARGS__)
#define EVDI_FATAL(...) EVDI_PRINTK(EVDI_LOGLEVEL_FATAL, "F", __VA_ARGS__)
#define EVDI_ERROR(...) EVDI_PRINTK(EVDI_LOGLEVEL_ERROR, "E", __VA_ARGS__)
#define EVDI_WARN(...) EVDI_PRINTK(EVDI_LOGLEVEL_WARN, "W", __VA_ARGS__)
#define EVDI_INFO(...) EVDI_PRINTK(EVDI_LOGLEVEL_INFO, "I", __VA_ARGS__)
#define EVDI_DEBUG(...) EVDI_PRINTK(EVDI_LOGLEVEL_DEBUG, "D", __VA_ARGS__)
#define EVDI_VERBOSE(...) EVDI_PRINTK(EVDI_LOGLEVEL_VERBOSE, "V", __VA_ARGS__)

#define EVDI_CHECKPT() EVDI_VERBOSE("L%d\n", __LINE__)
#define EVDI_ENTER() EVDI_VERBOSE("enter\n")
#define EVDI_EXIT() EVDI_VERBOSE("exit\n")

#endif // EVDI_DEBUG_H

