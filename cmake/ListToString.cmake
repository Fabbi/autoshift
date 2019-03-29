macro (strlist _result _input)
  string(REPLACE ";" ", " ${_result} "${${_input}}")
endmacro()
