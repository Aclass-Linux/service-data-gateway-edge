#include <cassert>

#include "DataGatewayEdge/Data/Repository.h"

int main() {
  DataGatewayEdge::Data::Repository repository;
  repository.save("alpha");
  assert(repository.records().size() == 1);
  return 0;
}
