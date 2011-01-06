package XmlPrinter;

sub new {
    my ($this, $handle) = @_; 
    my $class = ref($this) || $this;
    binmode $handle, ':encoding(utf8)';
    my $self = {fh => $handle, indent => ''};
    return bless $self, $class;
}

sub header {
    my ($self) = @_;
    my $fh = $self->{fh};
    print $fh '<?xml version="1.0" encoding="UTF-8"?>'."\n";
}

sub escape {
    my ($chars) = @_;
    $chars =~ s/&/&amp;/g;
    $chars =~ s/>/&gt;/g;
    $chars =~ s/</&lt;/g;
    return $chars;
}

sub start {
    my ($self, $name, $data, @attributes) = @_;
    my $fh = $self->{fh};
    if (not @attributes) {
        print $fh $self->{indent}."<$name>\n";
    } else {
        die "$name: Not a HASH reference" unless ref $data eq 'HASH';
        print $fh $self->{indent}."<$name";
        for my $attr (@attributes) {
            my $val = $data->{$attr};
            print $fh " $attr=\"$val\"" if defined $val;
        }
        print $fh ">\n";
    }
    $self->{indent} .= '  ';
}

sub end {
    my ($self, $name) = @_;
    chop $self->{indent};
    chop $self->{indent};
    my $fh = $self->{fh};
    print $fh $self->{indent}."</$name>\n";
}

sub properties {
    my ($self, $data, @props) = @_;
    my $indent = $self->{indent};
    my $fh = $self->{fh};
    for my $prop (@props) {
        my $val = $data->{$prop};
        next unless defined $val;
        if (not ref $val) {
            print $fh "$indent<$prop>".escape($val)."</$prop>\n";
        } elsif (ref $val eq 'ARRAY') {
            print $fh "$indent<$prop>".escape($_)."</$prop>\n" for @$val;
        } else {
            die "$prop: Not an ARRAY reference";
        }
    }
}



#### TEST ##### TEST ##### TEST ##### TEST ##### TEST ##### TEST ##### TEST ####

sub test {
    my $printer = XmlPrinter->new();
    my $data = { xmlns => "blah/blah", date => "today" };
    $printer->header();
    $printer->start("Persons", $data, qw( date ));
    $data = { id => "1", name => "James", nick => "Jimmy" };
    $printer->start("Person", $data, "id");
    $printer->properties($data, qw( name nick ));
    $printer->end("Person");
    $printer->end("Persons");
    
} 

#test;

1;

