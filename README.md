# Autocrud

This is a C++26 project that provides automatic CRUD operations into a PostgreSQL database.
It is currently very limited but it illustrates how one might set this up, and it does
work with a current gcc16. See my p2996_tests project for scripts and toolchain files to
build gcc16 and the clang Bloomberg fork in Linux if you need a compiler that supports
reflection.

## Limitations

This is designed to use classes that derive from fr::autocrud::Node, which provides
a unique ID for each record and associations between nodes. There is no inherent reason
why your tables need to derive from anything, but I've found the node/node_associations
stuff I provide from the object to be useful.

At the moment, the ID from Node is the primary key and autonode doesn't have a way to
set up other keys or relationships. I plan to add annotations to provide some extra
control over the database soon.

At the moment you can only store fairly basic types. You can store strings and things,
but arrays of other objects hasn't been tested and probably won't work. I may or
may not implement that at some point.

You can not store an autocrud class in an autocrud data object right now. I mean, you
can, but it won't work the way you want it to. I would like to detect them to establish
relationships between tables, but that'll take some work.

You can set node associations through the Node up/down lists and these do get recorded.
Storing one node at the moment will not store the other nodes, so you need to use
Node::traverse to store your nodes if you set those relationships up. Doing that is
kind of brittle if you need different Crud objects to store your different node types.
I anticipate this won't be extremely difficult to fix and intend to do so. See
the test NodeAssociationsBasic in IntegrationTests.cpp for an example of how traverse
works.

I don't currently have a method to read all related nodes out of the database. I'm
planning set up a factory to do that similar to the one in RequirementsManager, as
reading and writing graphs easily is a key element of this project.

## Usage

In addition to the following list, you can see the DerivedNodeBasicOneNode test in
IntegrationTests.cpp. That derives a very simple class from Node, redefines a field
name with an annotation and does some database things with a test node.

The Crud object provides the following operations:

 1. Crud::CreateTable - Creates a table if it doesn't already exist. It's safe to call repeatedly.
 2. Crud::DropTable - Drops a table. You can't call it repeatedly (unless you call CreateTable repeatedly too)
 3. Crud::Exists - Takes a object with its ID set and returns true if that ID is found in the database.
 4. Crud::Create - Creates a record in the table created for your class.
 5. Crud::Read - Takes an object with its ID set and populates the object if it finds it. It returns true if it did something.
 6. Crud::Update - Updates a record in the database
 7. Crud::Delete - Deletes a record from the database.

You can use the following annotations on the fiels in your object to affect your table:

 1. [[=fr::autocrud::DbIgnore{}]] Do not put this field into the database.
 2. [[=fr::autocrud::DbFieldName{std::define\_static\_string("...")}]] Rename the field in the database to "..."
 3. [[=fr::autocrud::DbFieldType{std::define\_static\)string("...")}]] Set the database field type
 
If you find these to be a bit long to type, you can include <fr/autocrud/Helpers.h> and use
these instead:

 1. [[=Ignore()]]
 2. [[="RenamedColumn"_ColumnName]]
 3. [[="VARCHAR(100)"_ColumnType]]
 
Autocrud does try to extrapolate database field types from c++ types if you don't set the
manually. See include/fr/CrudTypes.h for details.

Crud::Read and Crud::Update expect to be passed a node that has its node ID set.
The ID will be used to locate the record to operate on.

Basic steps:

 1. Define a structure that derives from node.
 2. Create an Crud object templated for your structure.
 3. Create a psqxx::connection to pass to the various CRUD operations.
 4. Call Create/Read/Update/Delete to CRUD things.
 
Crud will create a table named after your structure. I don't have an annotation to change this
at the moment. So the table for "struct Derived" will be "Derived". Your table fieldnames will
be the names of the elements in your structure, unless you rename them with DbFieldName.

# Warnings/Other

Be careful about running the Integration tests. I'm planning to create
and drop records and tables there pretty regularly, so you want to run
them on an isolated database. You have to enable them separately as an option
in the CMakeLists.txt file and it'll warn you if you do that too, but the
reflection toolchain generates a bunch of warnings too so it's easy to miss.

Most of the Crud operations call to the Crud specialization for Node to update
Node Records as well as your table. So if you write a node of some sort, you
can look in Node to see that the ID and Node type get record, and the
node_associations table that records IDs of nodes in the Up/Down vectors of your
node type. You get a fair bit of functionality from Node, which is probably
worth checking out. You can also use Node to implement singly or doubly linked
lists if you want to.

Adding things to node up/down lists does not reciprocate with the added node.
This is intentional; you can build one-way links in your graphs. Once I add a
factory to read an entire graph, one-way links would be a good way to keep
a read from exploding your entire database into memory if you use a lot of links.
In the RequirementsManager project I found it handy to create a specific node type
for graphs so you could search graphs on a keyword, grab the ID of the graph and
load all related elements. I'm planning to implement similar functionality in
this project.

I'm planning to do a fair bit of work on this, but the core functionality works quite well
at the moment so it seemed like a good time to upload it to github.
 
This is more-or-less a toy right now, but I expect it to be a lot more useful with
not much more work.

# TODOs

I should probably put in some annotations to establish additional indexes/keys
on a table for referential integrity and fast lookups of columns other than
id. This is pretty good now as a proof of concept, though.

