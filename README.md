# YottaDB PCRE Plugin

[Perl Compatible Regular Expressions (PCRE)](https://en.wikipedia.org/wiki/Perl_Compatible_Regular_Expressions) in YottaDB.

**Testing**
```
YDB>w $&pcre.test("match ME","/me/i")
1
YDB>w $&pcre.test("match ME","/me/")
0
```

**Replacing**
```
YDB>w $&pcre.replace("brown fox lazy dog","/(fox|dog)/g","cat")
brown cat lazy cat
YDB>w $&pcre.replace("brown fox lazy dog","/(fox|dog)/","cat")
brown cat lazy dog
USER YDB>
YDB>w $&pcre.replace("eyes","/(.)(.)/","$2$1")
yees
```

**Matching**
```
YDB>w $&pcre.match("brown fox lazy dog","/(?<first>\w+) (?<second>\w+)/g")
1
YDB>w $&pcre.get("first")," ",$&pcre.get("second")
brown fox
YDB>w $&pcre.next()
1
YDB>w $&pcre.get("first")," ",$&pcre.get("second")
lazy dog
YDB>w $&pcre.next()
0
```

**Matching (result in a record)**
```
YDB>w $&pcre.match("brown fox lazy dog","/(?<first>\w+) (?<second>\w+)/g","|")
brown|fox
YDB>w $&pcre.next()
lazy|dog
YDB>w $&pcre.next()

YDB>w $&pcre.end()
1
```

**Error handling**
```
YDB>w $&pcre.test("abc","/ab")
%YDB-E-XCRETNULLREF, Returned null reference from external call test
YDB>w $&pcre.error()
16386,&pcre.test,%PCRE-E-SLASH, Missing slash in search pattern
```
