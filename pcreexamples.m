; &pcre - YottaDB plugin with UTF-8 support built-in
;
; M API
;
;   $&pcre.error() - returns last error message in YottaDB runtime error message format (see $ZStatus)
;   $&pcre.test(text,search) - tests if search regular expression matches the text
;   $&pcre.replace(text,search,replace) - replaces matches search pattern with replace string (supports backreferences: $1, $2, ...)
;   $&pcre.match(text,search,seprator) - matches search regular expression on the text
;     $&pcre.get(indexOrGroupName) - returns matched substring
;     $&pcre.zvector(indexOrGroupName,separator) - returns firstIndex|lastIndex like in $ZExtract() for matched substring
;     $&pcre.isset(indexOrGroupName) - returns 1 if capture group was set during matching
;     &&pcre.next() - continues matching
;     $&pcre.end() - checks if there are (no) more matches possible
;

pcreexamples
  n tests
  d pcreError(.tests)
  d pcreTest(.tests)
  d pcreReplace(.tests)
  d pcreMatch(.tests)
  d pcreMatchRecord(.tests)
  d pcreMatchVector(.tests)
  d pcreMatchIsset(.tests)
  d summary(.tests)
  q


; $&pcre.error()  - returns last error massage
;
; NOTES:
; Last error message is cleared on every $&pcre.test(), $&pcre.replace() and $&pcre.match() call.

pcreError(tests)
  n exception,expected,found

  ; No error
  i $&pcre.test("","//")
  s found=$&pcre.error()
  s expected=""
  d checkEquality(.tests,expected,found)

  ; Missing slashes in search pattern
  d catch(.exception,"pcreError1")
  i $&pcre.test("","")
pcreError1
  d checkEquality(.tests,"%YDB-E-XCRETNULLREF, Returned null reference from external call test",.exception)
  s found=$&pcre.error()
  s expected="16386,&pcre.test,%PCRE-E-SLASH, Missing slash in search pattern"
  d checkEquality(.tests,expected,found)

  ; Invalid search options
  d catch(.exception,"pcreError2")
  i $&pcre.test("","//q")
pcreError2
  d checkEquality(.tests,"%YDB-E-XCRETNULLREF, Returned null reference from external call test",.exception)
  s found=$&pcre.error()
  s expected="16387,&pcre.test,%PCRE-E-OPT, Invalid options"
  d checkEquality(.tests,expected,found)

  ; No error (after errors before)
  i $&pcre.test("","//")
  s found=$&pcre.error()
  s expected=""
  d checkEquality(.tests,expected,found)

  q

pcreTest(tests)
  n exception,expected,found

  ; Simple match
  s found=$&pcre.test("The quick brown fox jumps over the lazy dog","/fox/")
  s expected=1
  d checkEquality(.tests,expected,found)

  ; No match
  s found=$&pcre.test("The quick brown fox jumps over the lazy dog","/FOX/")
  s expected=0
  d checkEquality(.tests,expected,found)

  ; Caseless match
  s found=$&pcre.test("The quick brown fox jumps over the lazy dog","/FOX/i")
  s expected=1
  d checkEquality(.tests,expected,found)

  ; Count words
  s found=$&pcre.test("The quick brown fox jumps over the lazy dog","/\b\w+/g")
  s expected=9
  d checkEquality(.tests,expected,found)

  ; Count words (UTF-8 mode)
  s found=$&pcre.test("Polish dąb is an oak in english","/\b\w+/g")
  s expected=7
  d checkEquality(.tests,expected,found)

  ; Count words (support for UTF-8 turned off, "/z" like in $ZLength())
  s found=$&pcre.test("Polish dąb is an oak in english","/\b\w+/gz")
  s expected=8
  d checkEquality(.tests,expected,found)

  ; Count characters (UTF-8)
  s found=$&pcre.test("dąb","/./g")
  s expected=3
  d checkEquality(.tests,expected,found)

  ; Count bytes (support for UTF-8 turned off, "/z" like in $ZLength())
  s found=$&pcre.test("dąb","/./gz")
  s expected=4
  d checkEquality(.tests,expected,found)

  ; Special case for every position (begin & end)
  s found=$&pcre.test("a","//g")
  s expected=2
  d checkEquality(.tests,expected,found)

  ; Special case for every position (begin, middle & end)
  s found=$&pcre.test("ab","//g")
  s expected=3
  d checkEquality(.tests,expected,found)

  ; Special case for global optional match
  s found=$&pcre.test("ab","/a?/g")
  s expected=3
  d checkEquality(.tests,expected,found)

  ; No arguments
  s found=$&pcre.test()
  s expected=0
  d checkEquality(.tests,expected,found)

  ; Empty text argument
  s found=$&pcre.test("")
  s expected=0
  d checkEquality(.tests,expected,found)

  ; Empty match (matches at the beginning)
  s found=$&pcre.test("","//")
  s expected=1
  d checkEquality(.tests,expected,found)

  ; Skipped text argument & empty match
  s found=$&pcre.test(,"//")
  s expected=1
  d checkEquality(.tests,expected,found)

  ; Missing slashes in search pattern
  d catch(.exception,"pcreTest1")
  i $&pcre.test("","")
pcreTest1
  d checkEquality(.tests,"%YDB-E-XCRETNULLREF, Returned null reference from external call test",.exception)
  s found=$&pcre.error()
  s expected="16386,&pcre.test,%PCRE-E-SLASH, Missing slash in search pattern"
  d checkEquality(.tests,expected,found)

  ; Skipped search pattern (defaults to a search pattern without slashes)
  d catch(.exception,"pcreTest2")
  i $&pcre.test("",)
pcreTest2
  d checkEquality(.tests,"%YDB-E-XCRETNULLREF, Returned null reference from external call test",.exception)
  s found=$&pcre.error()
  s expected="16386,&pcre.test,%PCRE-E-SLASH, Missing slash in search pattern"
  d checkEquality(.tests,expected,found)

  ; Extended pattern (invalid - no "/x")
  s found=$&pcre.test("The quick brown fox","/brown \s fox/")
  s expected=0
  d checkEquality(.tests,expected,found)

  ; Extended pattern (with "/x")
  s found=$&pcre.test("The quick brown fox","/brown \s fox/x")
  s expected=1
  d checkEquality(.tests,expected,found)

  ; More extended pattern (invalid - no "/xx", includes whitespaces in class)
  s found=$&pcre.test("fo fox","/fo  [ x ]/gx")
  s expected=2
  d checkEquality(.tests,expected,found)

  ; More extended pattern (with "/xx")
  s found=$&pcre.test("fo fox","/fo  [ x ]/gxx")
  s expected=1
  d checkEquality(.tests,expected,found)

  q

pcreReplace(tests)
  n exception,expected,found

  ; Replace once
  s found=$&pcre.replace("eyes","/e/","Y")
  s expected="Yyes"
  d checkEquality(.tests,expected,found)

  ; Replace globally
  s found=$&pcre.replace("eyes","/e/g","Y")
  s expected="YyYs"
  d checkEquality(.tests,expected,found)

  ; Backreferences
  s found=$&pcre.replace("eyes","/(.)(.)/g","$2$1")
  s expected="yese"
  d checkEquality(.tests,expected,found)

  ; Unknown backreference
  d catch(.exception,"pcreReplace1")
  i $&pcre.replace("eyes","/(.)(.)/g","$3")
pcreReplace1
  d checkEquality(.tests,"%YDB-E-XCRETNULLREF, Returned null reference from external call replace",.exception)
  s found=$&pcre.error()
  s expected="16390,&pcre.replace,%PCRE-E-SUBST, Substitution error: unknown substring"
  d checkEquality(.tests,expected,found)

  ; Strip all vowels (using more extended pattern - "/xx")
  s found=$&pcre.replace("The quick brown fox jumps over the lazy dog","/ [ a e i o u ] /xxg","")
  s expected="Th qck brwn fx jmps vr th lzy dg"
  d checkEquality(.tests,expected,found)

  ; Replace UTF-8 words
  s found=$&pcre.replace("dąb ddąąbb","/\w+/g","X")
  s expected="X X"
  d checkEquality(.tests,expected,found)

  ; Replace ASCII-7 words (support for UTF-8 turned off, \w matches ASCII-7)
  s found=$&pcre.replace("dąb ddąąbb","/\w+/gz","X")
  s expected="XąX XąąX"
  d checkEquality(.tests,expected,found)

  ; Special case for every position
  s found=$&pcre.replace("abc","//g","X")
  s expected="XaXbXcX"
  d checkEquality(.tests,expected,found)

  ; No arguments
  s found=$&pcre.replace()
  s expected=""
  d checkEquality(.tests,expected,found)

  ; Empty text argument
  s found=$&pcre.replace("")
  s expected=""
  d checkEquality(.tests,expected,found)

  ; Only text argument
  s found=$&pcre.replace("x")
  s expected="x"
  d checkEquality(.tests,expected,found)

  ; Only search argument
  s found=$&pcre.replace(,"//")
  s expected=""
  d checkEquality(.tests,expected,found)

  ; Empty search argument & no replace string
  s found=$&pcre.replace("abc","//")
  s expected="abc"
  d checkEquality(.tests,expected,found)

  ; Missing slashes in search pattern
  d catch(.exception,"pcreReplace2")
  i $&pcre.replace("","")
pcreReplace2
  d checkEquality(.tests,"%YDB-E-XCRETNULLREF, Returned null reference from external call replace",.exception)
  s found=$&pcre.error()
  s expected="16386,&pcre.replace,%PCRE-E-SLASH, Missing slash in search pattern"
  d checkEquality(.tests,expected,found)

  ; Skipped search pattern (defaults to a search pattern without slashes)
  d catch(.exception,"pcreReplace3")
  i $&pcre.replace(,)
pcreReplace3
  d checkEquality(.tests,"%YDB-E-XCRETNULLREF, Returned null reference from external call replace",.exception)
  s found=$&pcre.error()
  s expected="16386,&pcre.replace,%PCRE-E-SLASH, Missing slash in search pattern"
  d checkEquality(.tests,expected,found)

  q

pcreMatch(tests)
  n exception,expected,found


  ;; Match with capture (using group index)
  s found=$&pcre.match("The quick brown fox jumps over the lazy dog","/brown (fox) (\w+)/")
  s expected=1
  d checkEquality(.tests,expected,found)

  ; Retrieve whole match (index 0)
  s found=$&pcre.get(0)
  s expected="brown fox jumps"
  d checkEquality(.tests,expected,found)

  ; Retrieve first capture group
  s found=$&pcre.get(1)
  s expected="fox"
  d checkEquality(.tests,expected,found)

  ; Retrieve second capture group
  s found=$&pcre.get(2)
  s expected="jumps"
  d checkEquality(.tests,expected,found)

  ; Unknown capture group index
  d catch(.exception,"pcreMatch1")
  i $&pcre.get(3)
pcreMatch1
  d checkEquality(.tests,"%YDB-E-XCRETNULLREF, Returned null reference from external call get",.exception)
  s found=$&pcre.error()
  s expected="16394,&pcre.get,%PCRE-E-GROUP, Invalid capture group name or index"
  d checkEquality(.tests,expected,found)


  ;; Match with capture (using group name)
  s found=$&pcre.match("The quick brown fox jumps over the lazy dog","/brown (?<first>\w+) (?<second>\w+)/")
  s expected=1
  d checkEquality(.tests,expected,found)

  ; Retrieve first
  s found=$&pcre.get("first")
  s expected="fox"
  d checkEquality(.tests,expected,found)

  ; Retrieve second
  s found=$&pcre.get("second")
  s expected="jumps"
  d checkEquality(.tests,expected,found)

  ; Unknown capture group anme
  d catch(.exception,"pcreMatch2")
  i $&pcre.get("third")
pcreMatch2
  d checkEquality(.tests,"%YDB-E-XCRETNULLREF, Returned null reference from external call get",.exception)
  s found=$&pcre.error()
  s expected="16394,&pcre.get,%PCRE-E-GROUP, Invalid capture group name or index"
  d checkEquality(.tests,expected,found)

  ; Accessing capture groups by the index is still possible

  ; Retrieve whole match (index 0)
  s found=$&pcre.get(0)
  s expected="brown fox jumps"
  d checkEquality(.tests,expected,found)

  ; Retrieve first capture group
  s found=$&pcre.get(1)
  s expected="fox"
  d checkEquality(.tests,expected,found)

  ; Retrieve second capture group
  s found=$&pcre.get(2)
  s expected="jumps"
  d checkEquality(.tests,expected,found)



  ;;; Global match with capture
  s found=$&pcre.match("The quick brown fox jumps over","/(?<foo>\w+) (?<bar>\w+)/g")
  s expected=1  ; Matched, retireve results using $&pcre.get()
  d checkEquality(.tests,expected,found)

  ; Retrieve a foo
  s found=$&pcre.get("foo")
  s expected="The"
  d checkEquality(.tests,expected,found)

  ; Retrieve a bar
  s found=$&pcre.get("bar")
  s expected="quick"
  d checkEquality(.tests,expected,found)


  ;; Next match
  s found=$&pcre.next()
  s expected=1  ; Matched, retrieve results using $&pcre.get()
  d checkEquality(.tests,expected,found)

  ; Retrieve a foo
  s found=$&pcre.get("foo")
  s expected="brown"
  d checkEquality(.tests,expected,found)

  ; Retrieve a bar
  s found=$&pcre.get("bar")
  s expected="fox"
  d checkEquality(.tests,expected,found)


  ;; Next match
  s found=$&pcre.next()
  s expected=1  ; Matched, retrieve results using $&pcre.get()
  d checkEquality(.tests,expected,found)

  ; Retrieve a foo
  s found=$&pcre.get("foo")
  s expected="jumps"
  d checkEquality(.tests,expected,found)

  ; Retrieve a bar
  s found=$&pcre.get("bar")
  s expected="over"
  d checkEquality(.tests,expected,found)


  ;; Next match
  s found=$&pcre.next()
  s expected=0  ; No more matches, stop
  d checkEquality(.tests,expected,found)

  ; No capture groups are accesible now
  d catch(.exception,"pcreMatch3")
  i $&pcre.get("foo")
pcreMatch3
  d checkEquality(.tests,"%YDB-E-XCRETNULLREF, Returned null reference from external call get",.exception)
  s found=$&pcre.error()
  s expected="16393,&pcre.get,%PCRE-E-END, No more matches"
  d checkEquality(.tests,expected,found)

  ; Cannot continue with a next match
  d catch(.exception,"pcreMatch4")
  i $&pcre.next()
pcreMatch4
  d checkEquality(.tests,"%YDB-E-XCRETNULLREF, Returned null reference from external call next",.exception)
  s found=$&pcre.error()
  s expected="16393,&pcre.get,%PCRE-E-END, No more matches"
  d checkEquality(.tests,expected,found)


  n words
  s words="The quick brown fox jumps over the lazy dog"


  ;;; Global match with capture in a loop (using return value of $&pcre.match() and $&pcre.next())
  s found=$&pcre.match("The quick brown fox jumps over the lazy dog","/(?<word>\w+)/g")
  s expected=1
  d checkEquality(.tests,expected,found)

  n i
  f  q:'found  d
  . n word
  . s word=$&pcre.get("word")
  . d checkEquality(.tests,$p(words," ",$i(i)),word)
  . s found=$&pcre.next()
  d checkEquality(.tests,9,i)



  ;;; Global match with capture in a loop (using $&pcre.end())
  s found=$&pcre.match("The quick brown fox jumps over the lazy dog","/(?<word>\w+)/g")
  s expected=1
  d checkEquality(.tests,expected,found)

  n i
  f  q:$&pcre.end()  d
  . n word
  . s word=$&pcre.get("word")
  . d checkEquality(.tests,$p(words," ",$i(i)),word)
  . i $&pcre.next()
  d checkEquality(.tests,9,i)


  q

pcreMatchRecord(tests)
  n expected,found

  ; Match with result as a record (with "|" as a field separator)
  s found=$&pcre.match("The quick brown fox jumps over the lazy dog","/brown (fox) (\w+)/","|")
  s expected="fox|jumps"
  d checkEquality(.tests,expected,found)

  ; Match with result as a record (and whole match - using "/a")
  s found=$&pcre.match("The quick brown fox jumps over the lazy dog","/brown (fox) (\w+)/a","|")
  s expected="brown fox jumps|fox|jumps"
  d checkEquality(.tests,expected,found)

  n words
  s words="The&quick brown&fox jumps&over the&lazy dog"
  n i

  ; Global match with result as a record (with "&" as a field separator)
  s found=$&pcre.match("The quick brown fox jumps over the lazy dog","/(?<first>\w+) (?<second>\w+)/g","&")
  f  q:$&pcre.end()  d
  . s expected=$p(words," ",$i(i))
  . d checkEquality(.tests,expected,found)
  . s found=$&pcre.next()
  d checkEquality(.tests,4,i)

  q

pcreMatchVector(tests)
  n expected,found

  ; firstIndex|lastIndex (position vector) of matched substrings (with "|" as a record field separator)
  s found=$&pcre.match("The quick brown fox jumps over the lazy dog","/brown (fox) (\w+)/v","|")
  s expected="17|19|21|25"
  d checkEquality(.tests,expected,found)

  ;; Standard match
  s found=$&pcre.match("The quick brown fox jumps over the lazy dog","/brown (fox) (?<baz>\w+)/")
  s expected=1
  d checkEquality(.tests,expected,found)

  ; Position vector
  s found=$&pcre.zvector(1)
  s expected="17|19"
  d checkEquality(.tests,expected,found)

  ; Position vector (with "," as a field separator)
  s found=$&pcre.zvector("baz",",")
  s expected="21,25"
  d checkEquality(.tests,expected,found)

  n first,last,text
  s first=$p(found,",",1),last=$p(found,",",2)
  s text="The quick brown fox jumps over the lazy dog"

  ; Using returned indexes ($ZExtract())
  s $ze(text,first,last)="runs"
  s expected="The quick brown fox runs over the lazy dog"
  d checkEquality(.tests,expected,text)

  q

pcreMatchIsset(tests)
  n expected,found

  s found=$&pcre.match("ab","/ (?<foo>a)? (?<bar>X)? (?<baz>b)? (?<qux>X)? /x")
  s expected=1
  d checkEquality(.tests,expected,found)


  ;; foo is matched

  ; get() on matched capture group
  s found=$&pcre.get("foo")
  s expected="a"
  d checkEquality(.tests,expected,found)

  ; isset() on matched capture group
  s found=$&pcre.isset("foo")
  s expected=1
  d checkEquality(.tests,expected,found)


  ;; bar is not matched

  ; get() on umatched matched capture group
  s found=$&pcre.get("bar")
  s expected=""  ; empty string, no exception
  d checkEquality(.tests,expected,found)

  ; isset() on unmatched matched capture group
  s found=$&pcre.isset("bar")
  s expected=0
  d checkEquality(.tests,expected,found)


  ;; baz is matched

  ; get() on matched capture group
  s found=$&pcre.get("baz")
  s expected="b"
  d checkEquality(.tests,expected,found)

  ; isset() on matched capture group
  s found=$&pcre.isset("baz")
  s expected=1
  d checkEquality(.tests,expected,found)

  ;; qux is not matched

  ; get() on unmatched capture group
  s found=$&pcre.get("qux")
  s expected=""  ; empty string, no exception
  d checkEquality(.tests,expected,found)

  ; isset() on onmatched capture group
  s found=$&pcre.isset("qux")
  s expected=0
  d checkEquality(.tests,expected,found)

  q


catch(variable,label) ; setup exception handler: save exception into "variable" and goto "label"
  s variable=""
  n code,variableName
  s code=$st($st-1,"MCODE")
  s variableName=$p($p(code," catch(.",2),",",1)
  s $et="",$zt="s $ec="""","_variableName_"=$p($zs,"","",3,100) g "_label
  q

checkEquality(tests,expected,found)
  s $zt=""
  n result,place,context,i
  s result=(expected=found)
  s place=$st($st-1,"PLACE")
  f i=-1:-1:-4 s context(i)=$t(@($$labelOffset(place,i)))
  s context(-1)=$t(@($$labelOffset(place,-1)))
  w place," ... ",$s(result:"ok",1:"failed"),!
  i 'result d
  . w !
  . s i="" f  s i=$o(context(i)) q:i=""  w:context(i)'["; → " context(i),!
  . w !
  . w "    found: ",$s(found="":"(empty)",1:found),!
  . w " expected: ",$s(expected="":"(empty)",1:expected),!
  . w !
  s i=$i(tests)
  s tests(i,place)=result
  q

labelOffset(label,offset)
  q $p(label,"^",1)_"+"_offset_"^"_$p(label,"^",2)

summary(tests)
  n i,place,passed,failed
  s (passed,failed)=0
  s i="",place="" f  s i=$o(tests(i)) q:i=""  f  s place=$o(tests(i,place)) q:place=""  d
  . i tests(i,place),$i(passed)
  . i 'tests(i,place),$i(failed)
  i failed d
  . w !,"  Failed:  ",failed,".  Passed: ",passed,".",!!
  e  d
  . w !,"  All ",passed," tests passed.",!!
  q
