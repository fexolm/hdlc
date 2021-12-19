package main

import (
	"fmt"
	"log"

	"github.com/fexolm/hdlc/ast"
)

func main() {
	pkg, err := ast.ParsePackage("gates")
	if err != nil {
		log.Fatal(err)
	}

	fmt.Printf("%v\n", pkg)

	for _, c := range pkg.Chips {
		fmt.Printf("%+v\n", c)
		fmt.Printf("%+v\n", c.Interface)

		for _, i := range c.Interface.Inputs {
			fmt.Printf("%+v\n", i)
		}

		for _, i := range c.Interface.Outputs {
			fmt.Printf("%+v\n", i)
		}

		fmt.Printf("%+v\n", c.Impl)
	}

}
