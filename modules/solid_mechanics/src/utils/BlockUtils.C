#include "BlockUtils.h"

namespace BlockUtils
{

std::vector<SubdomainName>
getBlocksWithDefault(const InputParameters & params, const FEProblemBase & problem)
{
  if (params.isParamValid("block"))
    return params.get<std::vector<SubdomainName>>("block");
  else
    return problem.getDefaultBlocks();
}

} // namespace BlockUtils
