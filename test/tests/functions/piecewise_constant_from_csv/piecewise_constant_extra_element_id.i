# A material reads CSV data ordered by the value of an extra element integer ("grain_id") that
# differs from the subdomain ID. The grains are vertical strips (grain_id = 1, 2, 3 by x) while
# the subdomains are horizontal strips, so every subdomain spans several grains. With 1-based
# grain IDs, the elements must pick up column 3 of data_nearest.csv: 0.1, 4, 7 for grains 1, 2, 3.
[Mesh]
  [cmg]
    type = CartesianMeshGenerator
    dim = 2
    dx = '1 1 1'
    dy = '1 1'
    ix = '2 2 2'
    iy = '2 2'
    subdomain_id = '0 0 0
                    1 1 1'
  []
  [grain_id]
    type = ParsedExtraElementIDGenerator
    input = cmg
    expression = 'if(x < 1, 1, if(x < 2, 2, 3))'
    extra_elem_integer_name = grain_id
  []
[]

[UserObjects]
  [reader]
    type = PropertyReadFile
    prop_file_name = 'data_nearest.csv'
    read_type = 'extra_element_id'
    extra_element_id = grain_id
    use_zero_based_extra_element_id_indexing = false
    n_extra_element_ids = 3
    nprop = 4 # number of columns in CSV
  []
[]

[Functions]
  [by_grain]
    type = PiecewiseConstantFromCSV
    read_prop_user_object = 'reader'
    read_type = 'extra_element_id'
    column_number = '3'
  []
[]

[Materials]
  [grain_mat]
    type = GenericFunctionMaterial
    prop_names = 'grain_prop'
    prop_values = 'by_grain'
    constant_on = ELEMENT
  []
[]

[AuxVariables]
  [grain_aux]
    family = MONOMIAL
    order = CONSTANT
  []
[]

[AuxKernels]
  [grain_aux]
    type = MaterialRealAux
    variable = grain_aux
    property = grain_prop
    execute_on = 'INITIAL TIMESTEP_END'
  []
[]

[Postprocessors]
  [left]
    type = PointValue
    variable = grain_aux
    point = '0.5 1.5 0'
  []
  [middle]
    type = PointValue
    variable = grain_aux
    point = '1.5 0.5 0'
  []
  [right]
    type = PointValue
    variable = grain_aux
    point = '2.5 1.5 0'
  []
[]

[Problem]
  solve = false
[]

[Executioner]
  type = Steady
[]

[Outputs]
  csv = true
[]
