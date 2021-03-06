#ifndef PF_RESAMPLE_H
#define PF_RESAMPLE_H
#include "amcl/pf/pf.h"
#include "amcl/pf/pf_pdf.h"
#include "amcl/pf/pf_vector.h"
#include "amcl/pf/pf_kdtree.h"
#include "mcl/MCL.h"//for random number generator MCL::rng_


void pf_update_resample_kld(pf_t* pf);
void pf_update_resample_lowvariance(pf_t* pf_);
void pf_update_without_resample(pf_t* pf);

#endif //PF_RESAMPLE_H
