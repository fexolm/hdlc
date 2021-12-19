package irgen

import (
	"errors"

	"github.com/fexolm/hdlc/ast"
	"github.com/fexolm/hdlc/ir"
)

func GenerateIR(pkgs []*ast.Package, chipName string) (*ir.Module, error) {
	astChips := make(map[string]*ast.Chip)

	for _, pkg := range pkgs {
		for _, c := range pkg.Chips {
			astChips[pkg.Name+"."+c.Name] = c
		}
	}

	requiredChips, err := getRequiredChips(astChips[chipName], astChips)

	if err != nil {
		return nil, err
	}

	return genModule(astChips, requiredChips)
}

func genModule(astChips map[string]*ast.Chip, requiredChips []string) (*ir.Module, error) {
	chips := []*ir.Chip{}

	for _, name := range requiredChips {
		chip, err := compileChip(astChips[name])
		if err != nil {
			return nil, err
		}
		chips = append(chips, chip)
	}

	return &ir.Module{chips}, nil
}

func compileChip(chip *ast.Chip) (*ir.Chip, error) {
	res := &ir.Chip{}
	res.Type = &ir.ChipType{chip.Name, len(chip.Interface.Inputs), len(chip.Interface.Outputs)}
	res.Impl = &ir.ChipImpl{}

	for _, c := range chip.Impl.Stmts {

	}

	return res, nil
}

func getRequiredChips(chip *ast.Chip, astChips map[string]*ast.Chip) ([]string, error) {
	requiredChips := []string{chip.Name}
	visitedChips := map[string]struct{}{}
	visitQueue := []string{chip.Name}

	for len(visitQueue) > 0 {
		chipName := visitQueue[len(visitQueue)-1]
		if _, ok := visitedChips[chipName]; ok {
			continue
		}
		visitQueue = visitQueue[0 : len(visitQueue)-1]
		chip, ok := astChips[chipName]

		if !ok {
			return nil, errors.New("chip with required name not found")
		}

		for _, c := range chip.Impl.Stmts {
			if c, ok := c.(*ast.CreateChipExpr); ok {
				visitQueue = append(visitQueue, c.Name)
			}
		}
		visitedChips[chipName] = struct{}{}
		requiredChips = append(requiredChips, chipName)
	}

	return requiredChips, nil
}
