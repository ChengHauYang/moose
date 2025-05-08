#pragma once

#include "FEProblemBase.h"

#include <vector>

namespace BlockUtils
{

/**
 * Get list of blocks from input parameters or fallback to default blocks in the problem.
 *
 * @param params The input parameters (typically `_pars`)
 * @param problem The FEProblemBase instance (typically `*_problem`)
 * @return A vector of SubdomainNames either from the "block" parameter or the default blocks
 */
std::vector<SubdomainName> getBlocksWithDefault(const InputParameters & params,
                                                const FEProblemBase & problem);

} // namespace BlockUtils
