#include <li/metamap/make.hh>
#include <li/metamap/algorithms/substract.hh>
#include <cassert>

LI_SYMBOL(test1)
LI_SYMBOL(test2)
LI_SYMBOL(test3)
LI_SYMBOL(test4)

using namespace li;

int main()
{

  auto a = mmm(s::test1 = 12, s::test2 = 13, s::test3 = 13, s::test4 = 14);
  auto b = mmm(s::test2 = 12, s::test3 = 14);

  auto c = substract(a, b);

  assert(has_key(c, s::test1));
  assert(!has_key(c, s::test2));
  assert(!has_key(c, s::test3));
  assert(has_key(c, s::test4));
  assert(c.test1 == 12);
  assert(c.test4 == 14);
}
