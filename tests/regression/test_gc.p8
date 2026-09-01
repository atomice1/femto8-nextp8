pico-8 cartridge // http://www.pico-8.com
version 43
__lua__

#include test_fwk.lua

function alloc_some_mem()
  x = {}
  x.a = 123
  x.b = 234
  x.c = 345
  x.d = 456
  x.e = 567
  x.f = 678
  x.g = 789
  x.h = 901
  x.i = 012
  x.j = 123
  x.k = 234
  x.l = 345
  x.m = 456
  x.n = 567
  x.o = 678
  x.p = 789
  x.q = 890
  x.r = 901
  x.s = 012
  x.t = 123
  x.u = 234
  x.v = 345
  x.w = 456
  x.x = 567
  x.y = 678
  x.z = 789
  return x
end

function test_gc()
  --[[test_case("gc_reclaims_memory", function()
    printh("gc_reclaims_memory")
    start_mem = stat(0)
    check_gt(start_mem, 0)
    for i=1,2048 do alloc_some_mem() end
    end_mem = stat(99)
    check_gt(end_mem - start_mem, 2)
    iterations = 0
    while true do
      for i=1,100 do alloc_some_mem() end
      iterations += 1
      current_mem = stat(99)
      printh(current_mem)
      if current_mem < start_mem or current_mem - start_mem > 1024 or iterations > 10000 then
        break
      end
    end
    check_lt(iterations, 10000)
    final_mem = stat(0)
    check_ge(final_mem - start_mem, 0)
    check_lt(final_mem - start_mem, 2)
  end)]]--
  test_case("gc_only_reclaims_unreachable_memory", function()
    printh("gc_only_reclaims_unreachable_memory")
    y = {} n = 0
    start_mem = stat(0)
    check_gt(start_mem, 0)
    for i=1,2048 do y[n] = alloc_some_mem() n += 1 end
    end_mem = stat(99)
    check_gt(end_mem - start_mem, 2)
    iterations = 0
    while true do
      for i=1,100 do y[n] = alloc_some_mem() n += 1 end
      iterations += 1
      current_mem = stat(99)
      check_gt(current_mem, start_mem)
      if current_mem - start_mem > 1024 or iterations > 10000 then
        break
      end
    end
    check_lt(iterations, 10000)
    final_mem = stat(0)
    check_ge(final_mem - start_mem, 1024)
    y = {}
    final_mem = stat(0)
    check_ge(final_mem - start_mem, 0)
    check_lt(final_mem - start_mem, 2)
  end)
  test_case("can_allocate_up_to_2mb", function()
    printh("can_allocate_up_to_2mb")
    y = {} n = 0
    start_mem = stat(0)
    check_gt(start_mem, 0)
    for i=1,2048 do y[n] = alloc_some_mem() n += 1 end
    end_mem = stat(99)
    check_gt(end_mem - start_mem, 2)
    iterations = 0
    gc_done = false
    while true do
      y[n] = alloc_some_mem() n += 1
      iterations += 1
      current_mem = stat(99)
      check_gt(current_mem, start_mem)
      if current_mem - start_mem >= 1990 and not gc_done then
        gc_done = true
        current_mem = stat(0)
        check_gt(current_mem, start_mem)
      end
      if current_mem - start_mem >= 1990 or iterations > 200000 then
        break
      end
    end
    check_lt(iterations, 200000)
    final_mem = stat(0)
    check_ge(final_mem - start_mem, 1990)
    y = {}
    final_mem = stat(0)
    check_ge(final_mem - start_mem, 0)
    check_lt(final_mem - start_mem, 2)
  end)
end

test_suite("gc", test_gc)

summary()
