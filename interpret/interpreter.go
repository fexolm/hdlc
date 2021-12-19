package interpret

import (
	"github.com/fexolm/hdlc/ast"
)

type Interpreter struct {
	chips map[string]*Chip
}

type WireState bool 

type Chip struct {
	ir *ast.Chip
	interpreter *Interpreter
	wires map[string] WireState
}

func NewInterpreter(pkg *ast.Package) *Interpreter {
	chips :=make(map[string]*Chip)
	i := &Interpreter{chips}
	for _, c := range pkg.Chips {
		i.chips[c.Name] = i.makeChip(c)
	}
	return i
}

func (i *Interpreter) makeChip(impl *ast.Chip) *Chip {
	chip := &Chip{impl, i, make(map[string] WireState)}

	for _, c := range impl.Impl.Chips {
		for _, inp := range c.Results {
			chip.wires[inp.Name] = false
		}
	}

	return chip
}

func (i * Interpreter) runNand(inputs []WireState) []WireState {
	return []WireState {!(inputs[0] && inputs[1])}
}

func (c *Chip) run(inputs []WireState) []WireState {
	for i, inp := range c.ir.Interface.Inputs {
		c.wires[inp.Name] = inputs[i]
	}

	for _, ch := range c.ir.Impl.Chips {
		args := []WireState {}
		for _, a := range ch.Args {
			args = append(args, c.wires[a.Name])
		}

		outs := c.interpreter.Run(ch.ChipName, args)
		for i, o := range ch.Results {
			c.wires[o.Name] = outs[i]
		}
	}

	outs := []WireState{}
	for _, out := range c.ir.Impl.Results {
		outs = append(outs, c.wires[out.Name])
	}
	return outs
}

func (i *Interpreter) Run(entry string, inputs []WireState) []WireState {
	if entry == "builtin.Nand" {
		return i.runNand(inputs)
	}
	println(entry)
	return i.chips[entry].run(inputs)
}

