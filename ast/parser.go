package ast

import (
	"errors"
	"fmt"
	"io/ioutil"
	"os"
)

type scanner struct {
	src     []byte
	pos     int
	pkgName string
}

func newScanner(src []byte, pkgName string) *scanner {
	return &scanner{src, 0, pkgName}
}

func isLetter(c byte) bool {
	return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z')
}

func isSpace(c byte) bool {
	return c == ' ' || c == '\n' || c == '\r' || c == '\t'
}

func (s *scanner) readWord() []byte {
	if s.pos < len(s.src) {
		pos := s.pos
		for s.pos < len(s.src) && isLetter(s.src[s.pos]) {
			s.pos++
		}
		return s.src[pos:s.pos]
	} else {
		return nil
	}
}

func (s *scanner) readChar() *byte {
	if s.pos < len(s.src) {
		ch := s.src[s.pos]
		s.pos++
		return &ch
	} else {
		return nil
	}
}

func (s *scanner) peekChar() *byte {
	if s.pos < len(s.src) {
		return &s.src[s.pos]
	} else {
		return nil
	}
}

func (s *scanner) peekWord() []byte {
	if s.pos < len(s.src) {
		pos := s.pos
		for pos < len(s.src) && isLetter(s.src[pos]) {
			pos++
		}
		return s.src[s.pos:pos]
	} else {
		return nil
	}
}

func (s *scanner) skipSpaces() {
	for s.pos < len(s.src) && isSpace(s.src[s.pos]) {
		s.pos++
	}
}

func (s *scanner) readOneImport() (string, error) {
	s.skipSpaces()
	quote := s.readChar()
	if quote == nil || *quote != byte('"') {
		return "", errors.New("expected \" symbol")
	}
	pkg := s.readWord()
	if pkg == nil {
		return "", errors.New("expected import name, found eof")
	}
	quote = s.readChar()
	if quote == nil || *quote != byte('"') {
		return "", errors.New("expected \" symbol")
	}
	return string(pkg), nil
}

func (s *scanner) readImports() ([]string, error) {
	s.skipSpaces()
	importKw := s.readWord()
	if importKw == nil || string(importKw) != "import" {
		return nil, errors.New("package should start with import keryword")
	}
	s.skipSpaces()
	bracket := s.readChar()
	if bracket == nil || *bracket != '(' {
		return nil, errors.New("unexpected symbol in import statment")
	}

	imports := make([]string, 0)
	s.skipSpaces()
	for *s.peekChar() != ')' {
		if s.peekChar() == nil {
			return nil, errors.New("expected import name, found eof")
		}
		imp, err := s.readOneImport()
		if err != nil {
			return nil, err
		}
		imports = append(imports, imp)
		s.skipSpaces()
	}

	_ = s.readChar() // skip )
	return imports, nil
}

func (s *scanner) readChipSignarure() (*ChipSignarure, error) {
	ins, err := s.readParams()
	if err != nil {
		return nil, err
	}

	s.skipSpaces()
	// TODO: it's not a field list
	outs, err := s.readFieldList()
	if err != nil {
		return nil, err
	}

	return &ChipSignarure{Inputs: ins, Outputs: outs}, nil
}

func (s *scanner) readFieldList() ([]string, error) {
	results := []string{}
	for isLetter(*s.peekChar()) {
		w := s.readWord()
		if w == nil {
			return nil, errors.New("expected string list, found eof")
		}
		results = append(results, string(w))
		s.skipSpaces()
		if *s.peekChar() == ',' {
			_ = s.readChar()
			s.skipSpaces()
		}
	}
	return results, nil
}

func (s *scanner) readChipImpl() (*ChipBody, error) {
	valuesMap := make(map[string]*Value)

	bracket := s.readChar()
	if bracket == nil || *bracket != '{' {
		return nil, errors.New("expected ( symbol")
	}

	s.skipSpaces()
	if s.peekChar() == nil {
		return nil, errors.New("expected chip impl, found eof")
	}

	chips := []*ChipOp{}
	results := []*Value{}

	for *s.peekChar() != '}' {
		w := s.peekWord()

		if len(w) == 0 {
			return nil, errors.New("expected stmt, found eof1")
		}

		if string(w) == "return" {
			_ = s.readWord()
			s.skipSpaces()
			res, err := s.readFieldList()
			for _, v := range res {
				w, ok := valuesMap[v]
				if !ok {
					return nil, errors.New("returned wire not initialized")
				}
				results = append(results, w)
			}

			if err != nil {
				return nil, err
			}
			s.skipSpaces()

			fmt.Printf("results: %v\n", results)
		} else {
			names, err := s.readFieldList()

			outs := make([]*Value, 0)

			for _, n := range names {
				if w, ok := valuesMap[n]; ok {
					// TODO: assign wire type
					// TODO: assign parent
					outs = append(outs, w)
				} else {
					res := &Value{n, ""}
					outs = append(outs, res)
					valuesMap[n] = res
				}
			}

			if err != nil {
				return nil, err
			}
			s.skipSpaces()

			// Skipping :=
			// TODO: validate
			_ = s.readChar()
			_ = s.readChar()

			s.skipSpaces()

			// TODO: validate
			chipName := string(s.readWord())

			if *s.peekChar() == '.' {
				_ = s.readChar()
				// TODO: validate
				name := string(s.readWord())
				chipName = chipName + "." + name
			} else {
				chipName = s.pkgName + "." + chipName
			}

			s.skipSpaces()

			// skip bracket
			_ = s.readChar()
			s.skipSpaces()
			paramNames, err := s.readFieldList()
			args := []*Value{}
			if err != nil {
				return nil, err
			}

			for _, n := range paramNames {
				if w, ok := valuesMap[n]; ok {
					// TODO: add usage
					args = append(args, w)
				} else {
					arg := &Value{n, ""}
					args = append(args, arg)
					valuesMap[n] = arg
				}
			}
			// skip bracket
			_ = s.readChar()
			s.skipSpaces()
			chips = append(chips, &ChipOp{chipName, args, outs})
		}

		s.skipSpaces()
		if s.peekChar() == nil {
			return nil, errors.New("expected stmt, found eof2")
		}
	}

	bracket = s.readChar()
	if bracket == nil || *bracket != '}' {
		return nil, errors.New("expected ) symbol")
	}

	return &ChipBody{Chips: chips, Results: results}, nil
}

func (s *scanner) readParams() ([]*Value, error) {
	ws := make([]*Value, 0)

	bracket := s.readChar()
	if bracket == nil || *bracket != '(' {
		return nil, errors.New("expected ( symbol")
	}

	s.skipSpaces()
	for *s.peekChar() != ')' {
		name := s.readWord()
		if len(name) == 0 {
			return nil, errors.New("expected wire name, found eof")
		}

		s.skipSpaces()
		typ := s.readWord()
		if typ == nil || len(name) == 0 {
			return nil, errors.New("expected wire type, found eof")
		}

		ws = append(ws, &Value{string(name), Type(typ)})
		s.skipSpaces()
		ch := s.peekChar()
		if ch == nil {
			return nil, errors.New("expected , or ), found eof")
		}
		if *ch == ',' {
			_ = s.readChar()
			s.skipSpaces()
		}
	}

	bracket = s.readChar()
	if bracket == nil || *bracket != ')' {
		return nil, errors.New("expected ) symbol")
	}

	return ws, nil
}

func (s *scanner) readChip() (*Chip, error) {
	chipKw := s.readWord()
	if chipKw == nil || string(chipKw) != "chip" {
		return nil, errors.New("expected chip keryword, found smth else")
	}
	s.skipSpaces()
	chipName := s.readWord()
	if chipName == nil {
		return nil, errors.New("unexpected EOF")
	}
	s.skipSpaces()
	iface, err := s.readChipSignarure()
	if err != nil {
		return nil, err
	}
	s.skipSpaces()
	impl, err := s.readChipImpl()
	if err != nil {
		return nil, err
	}

	chip := &Chip{s.pkgName + "." + string(chipName), iface, impl}

	return chip, nil
}

func (s *scanner) readPackage(name string) (*Package, error) {
	s.skipSpaces()
	ch := s.peekChar()
	if ch == nil {
		return NewPackage(name, nil, nil), nil
	}
	// must be imports
	var imports []string = nil
	var err error = nil
	if *ch == 'i' {
		imports, err = s.readImports()
	}

	s.skipSpaces()
	chips := make([]*Chip, 0)
	for ch := s.peekChar(); ch != nil; ch = s.peekChar() {
		switch *ch {
		case 'c':
			c, err := s.readChip()
			if err != nil {
				return nil, err
			}
			chips = append(chips, c)
		default:
			return nil, errors.New("unexpected decl")
		}
		s.skipSpaces()
	}

	if err != nil {
		return nil, err
	}

	return NewPackage(name, imports, chips), nil
}

func ParsePackage(pkg string) (*Package, error) {
	f, err := os.Open(pkg + ".hdl")
	if err != nil {
		return nil, err
	}
	defer f.Close()
	buf, err := ioutil.ReadAll(f)
	if err != nil {
		return nil, err
	}
	s := newScanner(buf, pkg)
	return s.readPackage(pkg)
}
