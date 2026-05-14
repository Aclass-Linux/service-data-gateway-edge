#include <cassert>

#include "DataGatewayHub/Data/Repository.h"

int main() {
  DataGatewayHub::Data::Repository repository;
  repository.save("alpha");
  assert(repository.records().size() == 1);
  return 0;
}

