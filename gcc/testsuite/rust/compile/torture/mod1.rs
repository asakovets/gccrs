/*  dg-options "-frust-dump-all" } */
/*  dg-final {scan-file "gccrs.hir.dump" "TOTO" } } */

pub struct Bar {
    pub f:i32,
}

mod foomod {
    pub struct Foo {
        pub prout: i32,
    }
    pub fn totoprout() -> i32 {
        // Foo {
        //     prout: 4,
        // }
        return 3i32
    }
}

pub fn toto() {
}

// fn test() -> Bar {
//     Bar{f: 23i32}
// }

fn main() {
//    foomod::Foo{f:3i32}
    toto();
    let t = foomod::totoprout();
}
