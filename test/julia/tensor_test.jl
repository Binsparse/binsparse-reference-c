# SPDX-FileCopyrightText: 2024 Binsparse Developers
#
# SPDX-License-Identifier: BSD-3-Clause

using Finch;
using HDF5;

dir = pwd();

if length(ARGS) == 0
  tensor_test = joinpath(dir, "../../examples/tensor_test")
  test_files = joinpath(dir, "../../tensor_test_files")
else
  tensor_test = joinpath(dir, "../../examples/cpp/tensor_test-cpp")
  test_files = joinpath(dir, "../../tensor_test_files/cpp")
end

mkpath(test_files)

function tensortest(tensor::Tensor, input::AbstractString, output::AbstractString)
  print("tensor_test ", input, " -> ", output, "\n")
  fwrite(input, tensor)
  run(`$tensor_test $input $output`)
  output_tensor = fread(output)
  @assert tensor == output_tensor
end

tensortest(
  Tensor(
    Dense(SparseList{Int32}(SparseList{Int32}(Element{0.0,Float64,Int32}()))),
    fsprand(10, 10, 10, 0.1)
  ),
  joinpath(test_files, "input1.bsp.h5"),
  joinpath(test_files, "output1.bsp.h5")
)


tensortest(
  Tensor(
    Dense(SparseCOO{2}(Element(0.0))),
    fsprand(10, 10, 10, 0.1)
  ),
  joinpath(test_files, "input2.bsp.h5"),
  joinpath(test_files, "output2.bsp.h5")
)

tensortest(
  Tensor(
    Dense(Dense(Dense(Element{0.0,Float64,Int32}()))),
    fsprand(10, 10, 10, 0.1)
  ),
  joinpath(test_files, "input3.bsp.h5"),
  joinpath(test_files, "output3.bsp.h5")
)

tensortest(
  Tensor(
    SparseCOO{2}(Element(0.0)),
    fsprand(10, 10, 0.1)
  ),
  joinpath(test_files, "input4.bsp.h5"),
  joinpath(test_files, "output4.bsp.h5")
)

tensortest(
  Tensor(
    Dense(SparseList{Int32}(Element{0.0,Float64,Int32}())),
    [
      0 1 0 3;
      1 0 0 4;
    ]
  ),
  joinpath(test_files, "inputcsr.bsp.h5"),
  joinpath(test_files, "outputcsr.bsp.h5")
)
tensortest(
  Tensor(
    Dense(SparseList{Int32}(Element{0.0,Float64,Int32}())),
    fsprand(10, 10, 0.1)
  ),
  joinpath(test_files, "inputcsr2.bsp.h5"),
  joinpath(test_files, "outputcsr2.bsp.h5")
)

tensortest(
  Tensor(
    Dense(Dense(Element(0.0))),
    [
      1 1 2 3;
      6 1 5 4;
    ]
  ),
  joinpath(test_files, "inputdense.bsp.h5"),
  joinpath(test_files, "outputdense.bsp.h5")
)
