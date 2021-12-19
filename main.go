package main

import (
	"fmt"
	"log"

	"github.com/fexolm/hdlc/ast"
	"github.com/fexolm/hdlc/interpret"
)

func main() {
	pkg, err := ast.ParsePackage("gates")
	if err != nil {
		log.Fatal(err)
	}

	i := interpret.NewInterpreter(pkg)

	fmt.Printf("%v\n", i.Run("gates.AndThreeWay", []interpret.WireState{false, false, false}))
	fmt.Printf("%v\n", i.Run("gates.AndThreeWay", []interpret.WireState{false, false, true}))
	fmt.Printf("%v\n", i.Run("gates.AndThreeWay", []interpret.WireState{true, false, true}))
	fmt.Printf("%v\n", i.Run("gates.AndThreeWay", []interpret.WireState{true, true, true}))
}
